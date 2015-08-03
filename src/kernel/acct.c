#include "rscfl/kernel/acct.h"

#include <linux/hashtable.h>

#include "rscfl/config.h"
#include "rscfl/costs.h"
#include "rscfl/res_common.h"
#include "rscfl/kernel/cpu.h"
#include "rscfl/kernel/measurement.h"

//TODO(oc243): make thread safe.
static int num_tokens = 1;
static _Bool init;

static int num_tokens;

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
  acct_buf->syscall_id.id = current_pid_acct->ctrl->interest.syscall_id;
  // Initialise the subsys_accounting indices to -1, as they are used
  // to index an array, so 0 is valid.
  memset(acct_buf->acct_subsys, -1, sizeof(short) * NUM_SUBSYSTEMS);

  return acct_buf;
}

int should_acct(void)
{
  syscall_interest_t *interest;
  pid_acct *current_pid_acct;
  int i;
  struct rscfl_kernel_token *tbl_token;

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

  if ((interest->token == 0) || (interest->start_measurement)) {
    // Update buffer head.
    hash_for_each_possible(tokens, tbl_token, link, interest->token) {
      if (interest->token != tbl_token->id) {
        continue;
      }
      tbl_token->val = xen_buffer_hd();
    }
  }

  current_pid_acct->probe_data->syscall_acct = alloc_acct(current_pid_acct);

  if (!init) {
    hash_init(tokens);
    init = 1;
  }

  for (i = 0; i < current_pid_acct->ctrl->num_new_tokens; i++) {
    struct rscfl_kernel_token *token =
        kzalloc(GFP_KERNEL, sizeof(struct rscfl_kernel_token));
    // If we can't allocate a token it isn't the end of the world - we don't
    // promise to be able to. So keep going.
    if (token == NULL) {
      break;
    }
    token->id = num_tokens++;
    hash_add(tokens, &token->link, token->id);
    current_pid_acct->ctrl->new_tokens[i] = token->id;
  }
  current_pid_acct->ctrl->num_new_tokens = 0;
  
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
int clear_acct_next(void)
{
  pid_acct *current_pid_acct;
  syscall_interest_t *interest;

  preempt_disable();

  current_pid_acct = CPU_VAR(current_acct);
  interest = &current_pid_acct->ctrl->interest;
  // Clear the cached pointer to the struct accounting.
  current_pid_acct->probe_data->syscall_acct = NULL;
  // Reset the interest so we stop accounting.
  interest->syscall_id = 0;

  preempt_enable();
  return 0;
}
