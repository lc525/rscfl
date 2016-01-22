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
    if ((void *)(acct_buf + 1) >
        (void *)current_pid_acct->shared_buf->subsyses) {
      //acct_buf = current_pid_acct->shared_buf->acct;
      printk(KERN_WARNING "_should_acct: wraparound!<<<<<<<\n");
      return NULL;
    }
  }
  /*
   *if(IS_USER_TOKEN(acct_buf->token_id)) {
   *  current_pid_acct->token_ix[acct_buf->token_id]->account = NULL;
   *} else if(acct_buf->token_id == DEFAULT_TOKEN) {
   *  current_pid_acct->default_token->account = NULL;
   *}
   */
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
  volatile syscall_interest_t *interest;
  pid_acct *current_pid_acct;
  //int diff;

  preempt_disable();
  current_pid_acct = CPU_VAR(current_acct);

  // Need to test for ctrl == NULL as we first initialise current_pid_acct, on
  // the first mmap in rscfl_init, and then initialise the ctrl page in a second
  // mmap. If the second mmap hasn't happened yet, do nothing.
  if ((current_pid_acct == NULL) || (current_pid_acct->ctrl == NULL)) {
    // This pid has not (fully) initialised resourceful.
    preempt_enable();
    return 0;
  }

  interest = &current_pid_acct->ctrl->interest;
  if (!interest->syscall_id || interest->token_id == NULL_TOKEN) {
    // There are no interests registered for this pid.
    preempt_enable();
    return 0;
  }

  if(current_pid_acct->subsys_ptr == current_pid_acct->subsys_stack) {
    //printk(KERN_ERR "syscall!!\n");
    if(current_pid_acct->active_token->id != interest->token_id) {
      // we're swapping tokens to interest->token_id
      // printk(KERN_ERR "token swap from %d to %d\n", current_pid_acct->active_token->id, interest->token_id);
      if(IS_USER_TOKEN(interest->token_id)) {
        current_pid_acct->active_token =
          current_pid_acct->token_ix[interest->token_id];
      } else { // DEFAULT_TOKEN
        current_pid_acct->active_token = current_pid_acct->default_token;
      }
    }

    if(interest->first_measurement) {
      volatile rscfl_kernel_token *tk = current_pid_acct->active_token;
      if(tk->account != NULL && tk->account->token_id == tk->id ) {
        tk->account->in_use = 0;
      }
     tk->account = NULL;
    }

    current_pid_acct->probe_data->syscall_acct =
      current_pid_acct->active_token->account;

  }

/*
 *  if(current_pid_acct->subsys_ptr == current_pid_acct->subsys_stack) {
 *    // Consider token changes in user-space. If the user changes the token,
 *    // this will be reflected kernel-side after any ongoing system calls
 *    // have finished executing (and we have finished recording accounting data
 *    // for them)
 *    if(current_pid_acct->active_token->id != interest->token_id) {
 *      //swap tokens and the currently active syscall_acct
 *      printk(KERN_ERR "token swap from %d to %d\n", current_pid_acct->active_token->id, interest->token_id);
 *      if(IS_USER_TOKEN(interest->token_id)) {
 *        current_pid_acct->active_token =
 *          current_pid_acct->token_ix[interest->token_id];
 *      } else if(interest->token_id == NULL_TOKEN) {
 *        current_pid_acct->active_token = current_pid_acct->null_token;
 *        return 0;
 *      } else { // DEFAULT_TOKEN
 *        current_pid_acct->active_token = current_pid_acct->default_token;
 *      }
 *
 *      current_pid_acct->probe_data->syscall_acct =
 *        current_pid_acct->active_token->account;
 *    }
 *
 *    if(current_pid_acct->active_token == current_pid_acct->null_token)
 *      return 0;
 *
 *    // If we have received a TK_RESET, we need to record into a new
 *    // syscall_acct structure
 *    if(interest->first_measurement) {
 *      printk(KERN_ERR "first measurment!\n");
 *      current_pid_acct->probe_data->syscall_acct = NULL;
 *      if(current_pid_acct->active_token->account != NULL && current_pid_acct->active_token->account->in_use == 1) {
 *        printk(KERN_ERR "token %d should have been read!\n", current_pid_acct->active_token->id);
 *        current_pid_acct->active_token->account->in_use = 0;
 *      }
 *      current_pid_acct->active_token->account = NULL;
 *    }
 *  }
 */

  // We are now going to return 1, but need to find a struct accounting to
  // store the accounting data in.
  if (current_pid_acct->probe_data->syscall_acct) {
    preempt_enable();
    return 1;
  }

  current_pid_acct->probe_data->syscall_acct = alloc_acct(current_pid_acct);
  if(current_pid_acct->probe_data->syscall_acct == NULL) {
    interest->syscall_id = 0;
    interest->flags |= __ACCT_ERR;
    preempt_enable();
    return 0;
  }

  // diff = current_pid_acct->probe_data->syscall_acct - current_pid_acct->shared_buf->acct
  // printk(KERN_ERR "alloc_acct: %d for token %d\n", diff, current_pid_acct->active_token->id);
  if(interest->first_measurement) {
    volatile rscfl_kernel_token *tk = current_pid_acct->active_token;
    interest->first_measurement = 0;
    tk->val = xen_buffer_hd();
    tk->val2 = xen_current_sched_out();
    tk->account = current_pid_acct->probe_data->syscall_acct;
    tk->account->token_id = tk->id;
    //xen_clear_current_sched_out();
  } else {
    printk(KERN_ERR "Alloc but not first!");
  }

  preempt_enable();
  return 1;
}

int clear_acct_next(void)
{
  pid_acct *current_pid_acct;
  volatile syscall_interest_t *interest;

  preempt_disable();
  //printk("clear!\n");

  current_pid_acct = CPU_VAR(current_acct);
  interest = &current_pid_acct->ctrl->interest;
#ifdef RSCFL_BENCH
  // clear the acct/subsys memory if the ACCT_STOP was set
  //
  // For clearing things quickly dunring benchmark-ing, we make use of the
  // fact that for a given call and under no concurrent measurements  we'll
  // store the subsys data structures contiguously. This is deffinetely not true
  // in the general case!
  if((interest->flags & __BENCH_INTERNAL_CLR) != 0) {
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
  if( ((interest->flags & ACCT_START) == 0) ||
      ((interest->flags & ACCT_STOP ) != 0) ) {
    interest->syscall_id = 0;
  }

/*
 *#ifdef XEN_ENABLED
 *  current_pid_acct->active_token->val2 = -1 * xen_current_sched_out();
 *#endif
 */
  // If we're not aggregating in kernel-space, clear the cached pointer to the
  // struct accounting.
  if(current_pid_acct->ctrl->config.kernel_agg != 1) {
    current_pid_acct->probe_data->syscall_acct = NULL;
  }

  preempt_enable();
  return 0;
}
