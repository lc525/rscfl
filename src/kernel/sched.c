#include "rscfl/kernel/sched.h"

#include "linux/time.h"

#include "rscfl/costs.h"
#include "rscfl/kernel/acct.h"
#include "rscfl/kernel/cpu.h"
#include "rscfl/kernel/hasht.h"
#include "rscfl/kernel/probes.h"
#include "rscfl/kernel/shdw.h"
#include "rscfl/res_common.h"
#include "rscfl/kernel/subsys.h"

static void record_ctx_switch(pid_acct *p_acct, struct task_struct *task,
                              int values_add)
{
  struct accounting *acct;
  acct = p_acct->probe_data->syscall_acct;
  if (acct != NULL){
    struct subsys_accounting *subsys_acct;
    struct timespec ts;
    ru64 cycles;
    int err;

    err = get_subsys(*(p_acct->subsys_ptr-1), &subsys_acct);
    if (err < 0) {
      return;
    }

    // Snapshot counters
    cycles = rscfl_get_cycles();
    getrawmonotonic(&ts);

    if (values_add) {
      rscfl_timespec_add(&subsys_acct->sched.wct_out_local, &ts);
      subsys_acct->sched.cycles_out_local += cycles;
      subsys_acct->sched.run_delay += task->sched_info.run_delay;
    } else {
      rscfl_timespec_diff_comp(&subsys_acct->sched.wct_out_local, &ts);
      subsys_acct->sched.cycles_out_local -= cycles;
      subsys_acct->sched.run_delay -= task->sched_info.run_delay;
    }
  }
}

/* function that needs to be executed on every context switch
 * one extra hash table search on every context switch, for all processes
 * and one timestamp read per switch to or from a rscfl accounted path.
 */
void on_ctx_switch(void *ignore,
                   struct task_struct *prev,
                   struct task_struct *next)
{
  pid_t next_tid = next->pid;
  pid_acct *curr_acct = CPU_VAR(current_acct);
  if (curr_acct != NULL && curr_acct->ctrl->interest.token_id != NULL_TOKEN) {
    update_acct();
    record_ctx_switch(curr_acct, prev, 0);
  }

  hash_for_each_possible(CPU_TBL(pid_acct_tbl), curr_acct, link, next_tid) {
    if(curr_acct->pid == next_tid){
      CPU_VAR(current_acct) = curr_acct;
      if(curr_acct->ctrl->interest.token_id != NULL_TOKEN)
        record_ctx_switch(curr_acct, next, 1);
#if SHDW_ENABLED != 0
      // Switch shadow kernel if this process has a shadow kernel associated
      // with it.
      if (curr_acct->shdw_kernel) {
        if (shdw_switch_pages(curr_acct->shdw_kernel, curr_acct->shdw_pages)) {
          printk(KERN_ERR "Unable to switch to process's shadow kernel\n");
        }
      } else {
        shdw_reset();
      }
#endif
      return;
    }
  }

  // next_tid is not in the hash table (not a process using resourceful)
  CPU_VAR(current_acct) = NULL;
}


/* function that needs to be executed when a process/thread gets migrated from
 * cpu_from to cpu_to. Two hash lookups overhead for processes that do not use
 * resourceful.
 *
 * TODO(lc525, review discussion): we can turn this around and stop slowing
 * down others (only one hash lookup) if we accept two hash lookups for pids
 * using resourceful.
 *
 * this can execute on ANY cpu
 */
void on_cpu_switch(void *ignore,
                   struct task_struct *p,
                   int cpu_to)
{
  pid_acct *it;
  pid_t pid = p->pid;
  int cpu_from = task_cpu(p);

  /* We assume that if the process is long-lived, after a while all CPUs will
   * have its pid_acct structure within their hash table. So the fast path is to
   * just check for the existence of that entry in the destination CPU hash
   * table and return. on_ctx_switch will take care of the rest (updating
   * current_acct on the cpu)
   */
  hash_for_each_possible(per_cpu(pid_acct_tbl, cpu_to), it, link, pid) {
    if(it->pid == pid){
      return;
    }
  }


  hash_for_each_possible(per_cpu(pid_acct_tbl, cpu_from), it, link, pid) {
    if(it->pid == pid){
      hash_add(per_cpu(pid_acct_tbl, cpu_to), &it->link, pid);
      return;
    }
  }

  /* Process does not use resourceful  */
  return;
}


/* Remove the pid from the hash tables of any CPUs that might hold it.
 *
 * TODO(lc525): possible optimisation is to keep a set of what CPUs a pid has
 * been on, so that we minimise the number of hash table look-ups
 */
void on_task_exit(void *ignore, struct task_struct *p)
{
  int cpu_id;
  pid_acct *it;
  pid_t pid = p->pid;

  for_each_present_cpu(cpu_id) {
    hash_for_each_possible(per_cpu(pid_acct_tbl, cpu_id), it, link, pid) {
      if(it->pid == pid) {
        CPU_VAR(current_acct) = NULL;
        hash_del(&it->link);
        // Freeing the probe_data prevents the rscfl_handle from being reused
        // on other threads. We should _at least_ reset it or provide an option
        // to clean it from the API.
        // However, right now we don't have support for handle reuse, so we'll
        // free it here (on thread exit)
        if(it->probe_data) kfree(it->probe_data);
        kfree(it);
        break;
      }
    }
  }
}
