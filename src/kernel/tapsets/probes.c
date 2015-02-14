#include "rscfl/kernel/probes.h"

#include "linux/kprobes.h"

#include "rscfl/kernel/chardev.h"
#include "rscfl/kernel/cpu.h"
#include "rscfl/kernel/kprobe_manager.h"
#include "rscfl/kernel/perf.h"
#include "rscfl/kernel/stap_shim.h"
#include "rscfl/kernel/subsys_addr.h"

#define PROBE_LIST(_)               \
_(BLOCKLAYER)                       \
_(NETWORKINGIPV4IPV6)               \
_(NETWORKINGGENERAL)                \
_(EXT4FILESYSTEM)                   \
_(FILESYSTEMSVFSANDINFRASTRUCTURE)  \
_(NETWORKINGDRIVERS)                \
_(SECURITYSUBSYSTEM)

#define PROBES_AS_ADDRS(a) a##_ADDRS,
#define PROBES_AS_PRE_HANDLE(a) rscfl_pre_handler_##a,
#define PROBES_AS_RTN_HANDLE(a) rscfl_rtn_handler_##a,

int probes_init(void)
{
  kprobe_opcode_t **probe_addr;
  int rcd = 0, rcc = 0, rcp = 0, rckp = 0;
  kprobe_pre_handler_t pre_handler = pre_handler;
  int subsys_num;
  kprobe_opcode_t **probe_addrs_temp[] = {
    PROBE_LIST(PROBES_AS_ADDRS)
  };

  kretprobe_handler_t probe_pre_handlers_temp[] = {
    PROBE_LIST(PROBES_AS_PRE_HANDLE)
  };

  kretprobe_handler_t probe_post_handlers_temp[] = {
    PROBE_LIST(PROBES_AS_RTN_HANDLE)
  };

  // stap disables preemption even when running begin/end probes.
  // however, _rscfl_shim_init might sleep,
  // causing an BUG: scheduled in atomic section kernel error (for
  // sleeping with preemption disabled).
  //
  // because we know what we're doing, temporarily enable
  // scheduling here.
  // TODO(lc525): see if we need to restore IRQs as well
  //
  rcc = _rscfl_cpus_init();
  debugk("per-thread mmap alloc: Total: %lu, /acct: %d, /subsys: %lu\n",
         MMAP_BUF_SIZE, STRUCT_ACCT_NUM, ACCT_SUBSYS_NUM);
  preempt_enable();
  rcd = _rscfl_dev_init();
  rcp = rscfl_perf_init();
  rckp = rscfl_init_rtn_kprobes(
      probe_addrs_temp,
      sizeof(probe_pre_handlers_temp) / sizeof(kretprobe_handler_t),
      probe_pre_handlers_temp, probe_post_handlers_temp);
  preempt_disable();

  if (rcc) {
    printk(KERN_ERR "rscfl: cannot initialize per-cpu hash tables\n");
    return rcc;
  }
  if (rcd) {
    printk(KERN_ERR "rscfl: cannot initialize rscfl drivers\n");
    return rcd;
  }
  if (rcp) {
    printk(KERN_ERR "rscfl: cannot initialise perf\n");
    return rcp;
  }
  if (rckp) {
    printk("rscfl: failed to create probes: %d\n", rckp);
  }
  // Need to make kprobes work properly, and then return rckp.
  return 0;
}

int probes_cleanup(void)
{
  int rcd = 0, rcc = 0;

  // see comment in probes_init for why we need to explicitly enable
  // preemption here
  preempt_enable();
  rcd = _rscfl_dev_cleanup();
  rscfl_unregister_kprobes();
  preempt_disable();
  rcc = _rscfl_cpus_cleanup();

  if (rcd) {
    printk(KERN_ERR "rscfl: cannot cleanup rscfl drivers\n");
  }
  if (rcc) {
    printk(KERN_ERR "rscfl: cannot cleanup per-cpu hash tables\n");
  }
  return (rcd | rcc);
}

/*
 * Find the subsys_accounting for the current struct accounting with the
 * given subsys_id.
 *
 * Must be called with preemption disabled as we cache the current process.
 */
static int get_subsys(rscfl_subsys subsys_id,
                      struct subsys_accounting **subsys_acct_ret)
{
  struct accounting *acct;
  struct subsys_accounting *subsys_acct;
  pid_acct *current_pid_acct;
  rscfl_acct_layout_t *rscfl_mem;
  int subsys_offset;

  current_pid_acct = CPU_VAR(current_acct);
  acct = current_pid_acct->probe_data->syscall_acct;
  rscfl_mem = current_pid_acct->shared_buf;
  subsys_offset = acct->acct_subsys[subsys_id];
  if (subsys_offset == -1) {
    debugk("looking for a subsys for %d\n", subsys_id);
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
      debugk("rscfl: Unable to allocate memory for syscall accounting\n");
      return -ENOMEM;
    }
    // Now need to initialise the subsystem's resources to be 0.
    subsys_acct = &rscfl_mem->subsyses[subsys_offset];
    memset(subsys_acct, 0, sizeof(struct subsys_accounting));
    subsys_acct->in_use = 1;

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
int rscfl_subsystem_entry(rscfl_subsys subsys_id,
                          struct kretprobe_instance *probe)
{
  pid_acct *current_pid_acct;
  struct subsys_accounting *new_subsys_acct;
  struct subsys_accounting *curr_subsys_acct = NULL;
  int err;

  if (!should_acct()) {
    // Not accounting for this syscall, so exit, and don't set the return probe.
    return 1;
  }
  preempt_disable();
  current_pid_acct = CPU_VAR(current_acct);
  err = get_subsys(subsys_id, &new_subsys_acct);
  if (err < 0) {
    goto error;
  }
  // We running acct_next on this syscall.
  BUG_ON(current_pid_acct == NULL);  // As get_subsys != 0.
  if (current_pid_acct->curr_subsys != subsys_id) {
    rscfl_subsys *prev_subsys;
    // We're new in this subsystem.
    if (current_pid_acct->curr_subsys != -1) {
      err = get_subsys(current_pid_acct->curr_subsys, &curr_subsys_acct);
      if (err) {
        goto error;
      }
    }
    rscfl_perf_update_subsys_vals(curr_subsys_acct, new_subsys_acct);

    // Update the subsystem tracking info.
    prev_subsys = (rscfl_subsys *)probe->data;
    *prev_subsys = current_pid_acct->curr_subsys;
    current_pid_acct->curr_subsys = subsys_id;
    preempt_enable();
    return 0;
  }
  preempt_enable();
  return 1;
error:
  // If we hit an error (eg ENOMEM, then stop accounting).
  printk(KERN_ERR "rscfl: unexpected error in getting a subsystem.\n");
  if (current_pid_acct != NULL) {
    current_pid_acct->probe_data->syscall_acct->rc = err;
  }
  clear_acct_next();
  preempt_enable();
  // Don't register the exit probe.
  return 1;
}

void rscfl_subsystem_exit(rscfl_subsys subsys_id,
                          struct kretprobe_instance *probe)
{
  pid_acct *current_pid_acct = NULL;
  rscfl_subsys *prev_subsys = (rscfl_subsys *)probe->data;
  int err;

  if (!should_acct()) {
    return;
  }

  preempt_disable();
  current_pid_acct = CPU_VAR(current_acct);
  if ((current_pid_acct != NULL) &&
      (current_pid_acct->probe_data->syscall_acct)) {
    // This syscall is being accounted for.
    if (current_pid_acct->curr_subsys != -1) {
      struct subsys_accounting *subsys_acct;
      struct subsys_accounting *prev_subsys_acct = NULL;

      err = get_subsys(subsys_id, &subsys_acct);
      if (err) {
        goto error;
      }

      // Start counters again for the subsystem we're returning back to.
      if (*prev_subsys != -1) {
        err = get_subsys(*prev_subsys, &prev_subsys_acct);
        if (err) {
          goto error;
        }
      } else {
        clear_acct_next();
      }
      rscfl_perf_update_subsys_vals(subsys_acct, prev_subsys_acct);
      // Update subsystem tracking data.
      current_pid_acct->curr_subsys = *prev_subsys;
    }
  }
  preempt_enable();
  return;
error:
  current_pid_acct->probe_data->syscall_acct->rc = err;
  clear_acct_next();
  preempt_enable();
}
