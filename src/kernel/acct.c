/**** Notice
 * acct.c: rscfl source code
 *
 * Copyright 2015-2017 The rscfl owners <lucian.carata@cl.cam.ac.uk>
 *
 * This file is part of the rscfl open-source project: github.com/lc525/rscfl;
 * Its licensing is governed by the LICENSE file at the root of the project.
 **/

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
      debugk(RDBG_WARN, KERN_WARNING "_should_acct: wraparound!<<<<<<<\n");
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

int update_acct(void)
{
  volatile syscall_interest_t *interest;
  pid_acct *current_pid_acct;
#if !defined(NDEBUG) // debug-only
  int diff;
#endif

  current_pid_acct = CPU_VAR(current_acct);
  if(current_pid_acct == NULL) {
    return 1;
  }
  interest = &current_pid_acct->ctrl->interest;

  // update the active token
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
    if(tk != current_pid_acct->default_token &&
       tk->account != NULL &&
       tk->account->token_id == tk->id ) {
      // We want to reset the accounting for any token other than the default
      // one; If this is the first measurement, free the location storing
      // measurement data.
      tk->account->in_use = 0;
    }
   tk->account = NULL;
  }

  current_pid_acct->probe_data->syscall_acct =
    current_pid_acct->active_token->account;

  // existing struct accounting, aggregate into it
  if (current_pid_acct->probe_data->syscall_acct) {
    debugk(RDBG_FINER, KERN_WARNING "aggregate into token %d\n", current_pid_acct->active_token->id);
    return 0;
  }

  // Find a struct accounting to store the accounting data in.
  if(current_pid_acct->active_token == current_pid_acct->default_token){
    debugk(RDBG_FINER, KERN_WARNING "alloc for default token, first:%d\n", interest->first_measurement);
  }
  current_pid_acct->probe_data->syscall_acct = alloc_acct(current_pid_acct);
  if(current_pid_acct->probe_data->syscall_acct == NULL) {
    interest->syscall_id = 0;
    interest->flags |= __ACCT_ERR;
    debugk(RDBG_ERROR, KERN_ERR "error allocating token\n");
    return 1;
  }

#if !defined(NDEBUG) // debug-only
  diff = current_pid_acct->probe_data->syscall_acct -
         current_pid_acct->shared_buf->acct;
  debugk(RDBG_FINE, KERN_WARNING "alloc_acct: %d for token %d\n", diff, current_pid_acct->active_token->id);
#endif
  if(interest->first_measurement) {
    volatile rscfl_kernel_token *tk = current_pid_acct->active_token;
    interest->first_measurement = 0;
    tk->val = xen_buffer_hd();
    tk->val2 = xen_current_sched_out();
    tk->account = current_pid_acct->probe_data->syscall_acct;
    tk->account->token_id = tk->id;
    //xen_clear_current_sched_out();
  } else {
    debugk(RDBG_ERROR, KERN_ERR "Alloc subsys but not first measurement!\n");
  }

  return 0;
}

int clear_acct_next(void)
{
  pid_acct *current_pid_acct;
  volatile syscall_interest_t *interest;

  preempt_disable();
  debugk(RDBG_FINE, KERN_WARNING "clear!\n");

  current_pid_acct = CPU_VAR(current_acct);
  if (current_pid_acct == NULL) goto err;
  interest = &current_pid_acct->ctrl->interest;
  // If not a multi-syscall interest or if issued an explicit stop, reset the
  // interest so we stop accounting.
  if( ((interest->flags & ACCT_START) == 0) ||
      ((interest->flags & ACCT_STOP ) != 0) ) {
    interest->syscall_id = 0;
    interest->token_id = NULL_TOKEN;
    current_pid_acct->probe_data->syscall_acct = NULL;
  }

/*
 *#if XEN_ENABLED != 0
 *  current_pid_acct->active_token->val2 = -1 * xen_current_sched_out();
 *#endif
 */
  // If we're not aggregating in kernel-space, clear the cached pointer to the
  // struct accounting.
  if(current_pid_acct->ctrl->config.kernel_agg != 1) {
    current_pid_acct->probe_data->syscall_acct = NULL;
  }

err:
  preempt_enable();
  return 0;
}
