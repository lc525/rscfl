#include "rscfl/kernel/stap_shim.h"

#include <asm/atomic.h>
#include <linux/rwlock_types.h>
#include <linux/slab.h>
#include <linux/smp.h> // only here for smp_processor_id()
#include <linux/spinlock.h>

#include "rscfl/config.h"
#include "rscfl/costs.h"
#include "rscfl/res_common.h"
#include "rscfl/kernel/cpu.h"


long syscall_id_c;

static rwlock_t lock = __RW_LOCK_UNLOCKED(lock);

/**
 * if syscall_nr==-1 then we account for the next syscall, independent of which
 * syscall is executed.
 **/
int _should_acct(int syscall_nr, int probe_nest, const char *name,
                 struct accounting **acct)
{
  syscall_interest_t *interest;
  struct accounting *acct_buf;
  pid_acct *current_pid_acct;
  rscfl_shared_mem_layout_t *rscfl_shared_mem;

  preempt_disable();
  current_pid_acct = CPU_VAR(current_acct);

  // Need to test for ctrl != NULL as we first initialise current_pid_acct, on
  // the first mmap in rscfl_init, and then initialise the ctrl page in a second
  // mmap.
  if ((current_pid_acct == NULL) || (current_pid_acct->ctrl == NULL)) {
    // This pid has not initialised resourceful.
    preempt_enable();
    return 0;
  }

  interest = current_pid_acct->ctrl;
  if (!interest->syscall_id) {
    // There are no interests registered for this pid.
    preempt_enable();
    return 0;
  }

  // We are now going to return 1, but need to find a struct accounting to
  // store the accounting data in.

  if (current_pid_acct->probe_data->syscall_acct) {
    // We have already called this function for the current syscall, so we
    // should reuse the struct accounting that we already found.

    *acct = current_pid_acct->probe_data->syscall_acct;
    preempt_enable();
    return 1;
  }

  // Find a free struct accounting in the shared memory that we can
  // use.
  rscfl_shared_mem = current_pid_acct->shared_buf;
  acct_buf = rscfl_shared_mem->acct;
  BUG_ON(!acct_buf);
  while (acct_buf->in_use) {
    debugk("in use: %p for (pid, id):(%d, %lu)\n", (void *)acct_buf,
           current_pid_acct->pid, acct_buf->syscall_id.id);
    acct_buf++;
    if ((void *)acct_buf + sizeof(struct accounting) >
        (void *)current_pid_acct->shared_buf->subsyses) {
      acct_buf = current_pid_acct->shared_buf->acct;
      debugk("_should_acct: wraparound!<<<<<<<\n");
      break;
    }
  }
  // We have a free struct accounting now, so use it.
  current_pid_acct->probe_data->syscall_acct = acct_buf;
  current_pid_acct->probe_data->nest_level = 0;
  acct_buf->in_use = 1;
  acct_buf->syscall_id.id = interest->syscall_id;
  debugk("syscall_id=%lu\n", interest->syscall_id);
  // Initialise the subsys_accounting indices to -1, as they are used
  // to index an array, so 0 is valid.
  memset(acct_buf->acct_subsys, -1, sizeof(short) * NUM_SUBSYSTEMS);
  debugk("_should_acct %s: (yes, nr %lu) %d, into %p\n", name,
         interest->syscall_id, current_pid_acct->pid, (void *)acct_buf);
  *acct = acct_buf;
  preempt_enable();
  return 1;
}

/**
 * if syscall_nr==-1 then all resource consumption requests for the given pid
 * are cleared.
 *
 * if pid==-1 then syscall_nr will be cleared regardless of its associated pid
 *
 * if pid==-1 && syscall_nr==-1 then the resource consumption list is cleared
 **/
int _clear_acct_next(int syscall_nr)
{
  pid_acct *current_pid_acct;
  syscall_interest_t *interest;

  preempt_disable();

  current_pid_acct = CPU_VAR(current_acct);
  interest = current_pid_acct->ctrl;
  // Clear the cached pointer to the struct accounting.
  current_pid_acct->probe_data->syscall_acct = NULL;
  // Reset the interest so we stop accounting.
  interest->syscall_id = 0;

  preempt_enable();
  return 0;
}
