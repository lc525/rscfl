#include "rscfl/kernel/probes.h"

#include "linux/kprobes.h"

#include "rscfl/kernel/cpu.h"
#include "rscfl/kernel/kamprobes.h"
#include "rscfl/kernel/measurement.h"
#include "rscfl/kernel/priv_kallsyms.h"
#include "rscfl/kernel/shdw.h"
#include "rscfl/kernel/stap_shim.h"
#include "rscfl/kernel/subsys_addr.h"

#define PROBE_LIST(_)               \
_(BLOCKLAYER)                       \
_(NETWORKINGIPV4IPV6)               \
_(NETWORKINGGENERAL)                \
_(FILESYSTEMSVFSANDINFRASTRUCTURE)  \
_(NETWORKINGDRIVERS)                \
_(SECURITYSUBSYSTEM)                \
_(XENHYPERVISORINTERFACE)           \
_(XENINTERRUPTS)

#define PROBES_AS_ADDRS(a) a##_ADDRS,
#define PROBES_AS_SYSCALL_TYPE(a) a##_INTERNAL_SYSCALL,
#define PROBES_AS_PRE_HANDLE(a) rscfl_pre_handler_##a,
#define PROBES_AS_RTN_HANDLE(a) rscfl_rtn_handler_##a,

int probes_init(void)
{
  kprobe_pre_handler_t pre_handler = pre_handler;
  u8 **probe_addrs_temp[] = {PROBE_LIST(PROBES_AS_ADDRS)};
  char *syscall_type_temp[] = {PROBE_LIST(PROBES_AS_SYSCALL_TYPE)};

  int i, rc, failures = 0;
  unsigned long flags;

  void (*probe_pre_handlers_temp[])(void) = {PROBE_LIST(PROBES_AS_PRE_HANDLE)};
  void (*probe_post_handlers_temp[])(void) = {PROBE_LIST(PROBES_AS_RTN_HANDLE)};

  int num_subsys = sizeof(probe_pre_handlers_temp) / sizeof(kretprobe_handler_t);

  kamprobes_init(RSCFL_NUM_PROBES);
  local_irq_save(flags);
  for (i = 0; i < num_subsys; i++) {
    u8 **sub_addr = probe_addrs_temp[i];
    int j = 0;
    while (*sub_addr) {
      rc = kamprobes_register(sub_addr, syscall_type_temp[i][j],
                              probe_pre_handlers_temp[i],
                              probe_post_handlers_temp[i]);
      if (rc) {
        failures++;
      }
      sub_addr++;
      j++;
    }
  }
  local_irq_restore(flags);
  return failures;
}

int probes_cleanup(void)
{
  kamprobes_unregister_all();
  return 0;
}

/*
 * Find the subsys_accounting for the current struct accounting with the
 * given subsys_id.
 *
 * Must be called with preemption disabled as we cache the current process.
 */
int get_subsys(rscfl_subsys subsys_id,
               struct subsys_accounting **subsys_acct_ret)
{
  struct accounting *acct;
  struct subsys_accounting *subsys_acct;
  pid_acct *current_pid_acct;
  rscfl_acct_layout_t *rscfl_mem;
  int subsys_offset;

  current_pid_acct = CPU_VAR(current_acct);
  BUG_ON(current_pid_acct == NULL);

  acct = current_pid_acct->probe_data->syscall_acct;
  rscfl_mem = current_pid_acct->shared_buf;
  subsys_offset = acct->acct_subsys[subsys_id];
  if (subsys_offset == -1) {
    // Need to find space in the page where we can store the subsystem.
    subsys_acct = rscfl_mem->subsyses;
    // Walk through the subsyses, being careful not to wonder of the end of
    // our memory.
    while (subsys_acct - rscfl_mem->subsyses <= ACCT_SUBSYS_NUM) {
      if (!subsys_acct->in_use) {
        // acct_subsys is an index that describes the offset from the start of
        // subsyses as measured by number of struct subsys_accountings.
        // Recall that this is done as we need consistent indexing between
        // userspace and kernel space.
        subsys_offset = subsys_acct - rscfl_mem->subsyses;
        acct->acct_subsys[subsys_id] = subsys_offset;
        acct->nr_subsystems++;
        break;
      } else {
        subsys_acct++;
      }
    }
    if (subsys_offset == -1) {
      // We haven't found anywhere in the shared page where we can store
      // this subsystem.
      printk(KERN_ERR
             "rscfl: Unable to allocate memory for syscall accounting\n");
      return -ENOMEM;
    }
    // Now need to initialise the subsystem's resources to be 0.
    subsys_acct = &rscfl_mem->subsyses[subsys_offset];
    memset(subsys_acct, 0, sizeof(struct subsys_accounting));
    subsys_acct->in_use = 1;
    subsys_acct->sched.xen_credits_min = INT_MAX;
    subsys_acct->sched.xen_credits_max = INT_MIN;

  } else {
    subsys_acct = &rscfl_mem->subsyses[subsys_offset];
  }
  *subsys_acct_ret = subsys_acct;
  return 0;
}

/*
 * Returns 0 if we have entered a new subsystem, without errors.
 * Returns 1 if we are not in a new subsystem, or there is an error.
 */
void rscfl_subsystem_entry(rscfl_subsys subsys_id)
{
  pid_acct *current_pid_acct;
  struct subsys_accounting *new_subsys_acct;
  // Needs to be initialised to NULL so that if there is no current subsys,
  // we pass NULL to rscfl_perf_update_subsys_vals, which is well-handled.
  struct subsys_accounting *curr_subsys_acct = NULL;
  int err;

  if (subsys_id == XENINTERRUPTS) {
    // Keep a count of how many event channel events we have fired.
    no_evtchn_events++;
    // No need to keep running - this is a common operation that isn't crossing
    // a subsystem boundary, so we don't want the overheads of all of the other
    // stuff.
    return;
  }

  preempt_disable();
  current_pid_acct = CPU_VAR(current_acct);
  // Don't continue if we're already running a probe or we may double-fault.
  if ((current_pid_acct == NULL) || (current_pid_acct->executing_probe)) {
    return;
  }
  current_pid_acct->executing_probe = 1;

  if (!should_acct()) {
    // Not accounting for this syscall, so exit, and don't set the return probe.
    current_pid_acct->executing_probe = 0;
    return;
  }

  err = get_subsys(subsys_id, &new_subsys_acct);
  if (err < 0) {
    goto error;
  }
  BUG_ON(current_pid_acct == NULL);  // As get_subsys != 0.

  if (current_pid_acct->subsys_ptr != current_pid_acct->subsys_stack) {
    // This is not the first subsystem of the syscall, so we want to update
    // the values in the previous subsystem.

    // The current subsys is just below the subsys_ptr.
    err = get_subsys(current_pid_acct->subsys_ptr[-1], &curr_subsys_acct);
    if (err) {
      goto error;
    }
  }
  rscfl_counters_update_subsys_vals(curr_subsys_acct, new_subsys_acct);

  // Update the subsystem tracking info.
  *(current_pid_acct->subsys_ptr) = subsys_id;
  current_pid_acct->subsys_ptr++;

  preempt_enable();
  current_pid_acct->executing_probe = 0;
  return;

error:
  // If we hit an error (eg ENOMEM, then stop accounting).
  printk(KERN_ERR "rscfl: unexpected error in getting a subsystem.\n");
  if (current_pid_acct != NULL) {
    current_pid_acct->probe_data->syscall_acct->rc = err;
    current_pid_acct->executing_probe = 0;
  }
  clear_acct_next();
  preempt_enable();
  return;
}

void rscfl_subsystem_exit(rscfl_subsys subsys_id)
{
  pid_acct *current_pid_acct = NULL;
  struct subsys_accounting *subsys_acct;
  struct subsys_accounting *prev_subsys_acct = NULL;

  int err;

  if (subsys_id == XENINTERRUPTS) {
    // No need to keep running - this is a common operation that isn't crossing
    // a subsystem boundary, so we don't want the overheads of all of the other
    // stuff.
    return;
  }

  preempt_disable();
  current_pid_acct = CPU_VAR(current_acct);

  if ((current_pid_acct == NULL) || (current_pid_acct->executing_probe)) {
    return;
  }
  current_pid_acct->executing_probe = 1;

  if (!should_acct()) {
    current_pid_acct->executing_probe = 0;
    return;
  }

  if ((current_pid_acct != NULL) &&
      (current_pid_acct->probe_data->syscall_acct)) {
    // This syscall is being accounted for.

    // Now point at the frame of the subsystem being left.
    *(current_pid_acct->subsys_ptr) = subsys_id;
    current_pid_acct->subsys_ptr--;

    err = get_subsys(subsys_id, &subsys_acct);
    if (err) {
      goto error;
    }

    // Start counters again for the subsystem we're returning back to.
    if (current_pid_acct->subsys_ptr > current_pid_acct->subsys_stack) {
      err = get_subsys(current_pid_acct->subsys_ptr[-1], &prev_subsys_acct);
      if (err) {
        goto error;
      }
    } else {
      clear_acct_next();
    }
    rscfl_counters_update_subsys_vals(subsys_acct, prev_subsys_acct);
    // Update subsystem tracking data.
  }
  current_pid_acct->executing_probe = 0;
  preempt_enable();
  return;
error:
  current_pid_acct->probe_data->syscall_acct->rc = err;
  clear_acct_next();
  current_pid_acct->executing_probe = 0;
  preempt_enable();
}
