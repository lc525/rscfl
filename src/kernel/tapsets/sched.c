#include "rscfl/kernel/sched.h"

#include "rscfl/kernel/cpu.h"
#include "rscfl/kernel/hasht.h"

/* function that needs to be executed on every context switch
 * one extra hash table search on every context switch, for all processes
 */
void on_ctx_switch(pid_t next_tid)
{
  pid_acct *it;

  hash_for_each_possible(CPU_TBL(pid_acct_tbl), it, link, next_tid) {
    if(it->pid == next_tid){
      CPU_VAR(current_acct) = it;
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
void on_cpu_switch(int cpu_from, int cpu_to, pid_t pid)
{
  pid_acct *it;

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
void on_task_exit(pid_t pid)
{
  int cpu_id;
  pid_acct *it;

  for_each_present_cpu(cpu_id) {
    hash_for_each_possible(per_cpu(pid_acct_tbl, cpu_id), it, link, pid) {
      if(it->pid == pid) {
        /*
         * no more probes will be firing on this thread, but perhaps in
         * userspace we want to reuse the rscfl_handle for a new thread?
         *
         * kfree(it->probe_data);
         *
         * freeing acct_buf also prove tricky: what if the thread
         * dies but other threads want to access its resource accounting data?
         *
         * kfree(it->acct_buf);
         */
        hash_del(&it->link);
        break;
      }
    }
  }
}
