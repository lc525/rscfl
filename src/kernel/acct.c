#include "rscfl/kernel/acct.h"

#include <linux/compiler.h>
#include <linux/hashtable.h>

#include "rscfl/config.h"
#include "rscfl/costs.h"
#include "rscfl/res_common.h"
#include "rscfl/kernel/cpu.h"
#include "rscfl/kernel/measurement.h"
#include "rscfl/kernel/xen.h"

static struct accounting *alloc_acct(pid_acct *current_pid_acct)
{
  rscfl_acct_layout_t *rscfl_shared_mem = current_pid_acct->shared_buf;
  struct accounting *acct_buf = rscfl_shared_mem->acct;

  BUG_ON(!acct_buf);
  while (acct_buf->in_use) {
    acct_buf++;
    if ((void *)acct_buf + sizeof(struct accounting) >
        (void *)current_pid_acct->shared_buf->subsyses) {
      acct_buf = current_pid_acct->shared_buf->acct;
      printk(KERN_WARNING "_should_acct: wraparound!<<<<<<<\n");
      break;
    }
  }
  acct_buf->in_use = 1;
  acct_buf->nr_subsystems = 0;
  acct_buf->syscall_id = current_pid_acct->ctrl->interest.syscall_id;
  // Initialise the subsys_accounting indices to -1, as they are used
  // to index an array, so 0 is valid.
  memset(acct_buf->acct_subsys, -1, sizeof(short) * NUM_SUBSYSTEMS);

  return acct_buf;
}

int should_acct(void)
{
  syscall_interest_t *interest;
  pid_acct *current_pid_acct;

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

  interest = &current_pid_acct->ctrl->interest;
  if (!interest->syscall_id) {
    // There are no interests registered for this pid.
    preempt_enable();
    return 0;
  }

  // We are now going to return 1, but need to find a struct accounting to
  // store the accounting data in.

  if (current_pid_acct->probe_data->syscall_acct) {
    // We have already called this function for the current syscall.
    preempt_enable();
    return 1;
  }

  current_pid_acct->probe_data->syscall_acct = alloc_acct(current_pid_acct);

  if(interest->start_measurement) {
    rscfl_kernel_token *tk;
    if ((interest->token_id == NO_TOKEN)) {
      tk = current_pid_acct->default_token;
    } else {
      tk = current_pid_acct->token_ix[interest->token_id];
    }
    tk->val = xen_buffer_hd();
    tk->account = current_pid_acct->probe_data->syscall_acct;
    current_pid_acct->active_token = tk;
  }

  preempt_enable();
  return 1;
}

int clear_acct_next(void)
{
  pid_acct *current_pid_acct;
  syscall_interest_t *interest;

  preempt_disable();

  current_pid_acct = CPU_VAR(current_acct);
  interest = &current_pid_acct->ctrl->interest;
#ifdef RSCFL_BENCH
  // clear the acct/subsys memory if the IST_CLEAR_FLAG was set
  //
  // For clearing things quickly dunring benchmark-ing, we make use of the
  // fact that for a given call and under no concurrent measurements  we'll
  // store the subsys data structures contiguously. This is deffinetely not true
  // in the general case!
  if((interest->flags & IST_CLEAR_ACCT) != 0) {
    int i;
    struct subsys_accounting *sa = current_pid_acct->shared_buf->subsyses;

    for(i = 0; i < ACCT_SUBSYS_NUM; i++) {
      if(likely(sa[i].in_use != 0))
        sa[i].in_use = 0;
      else
        break;
    }
    current_pid_acct->probe_data->syscall_acct->in_use = 0;
  }
#endif
  // If not a multi-syscall interest or if issued an explicit stop, reset the
  // interest so we stop accounting.
  if((interest->flags & IST_START) == 0 ||
     (interest->flags & IST_STOP ) != 0) {
    interest->syscall_id = 0;
  }

  // If we're not aggregating in kernel-space, clear the cached pointer to the
  // struct accounting.
  if(current_pid_acct->ctrl->config.kernel_agg != 1) {
    current_pid_acct->probe_data->syscall_acct = NULL;
    current_pid_acct->active_token->account = NULL;
  }

  // Consider token changes in user-space. If the user changes the token,
  // this will be reflected kernel-side after any ongoing system calls
  // have finished executing (and we have finished recording accounting data
  // for them)
  if(unlikely(current_pid_acct->active_token->id != interest->token_id)){
    //swap tokens and the currently active syscall_acct
    current_pid_acct->active_token =
      current_pid_acct->token_ix[interest->token_id];
    current_pid_acct->probe_data->syscall_acct =
      current_pid_acct->token_ix[interest->token_id]->account;
  }

  preempt_enable();
  return 0;
}
