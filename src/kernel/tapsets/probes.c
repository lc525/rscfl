#include "rscfl/kernel/probes.h"

#include "linux/kprobes.h"

#include "rscfl/kernel/chardev.h"
#include "rscfl/kernel/cpu.h"
#include "rscfl/kernel/kprobe_manager.h"
#include "rscfl/kernel/perf.h"
#include "rscfl/kernel/stap_shim.h"
#include "rscfl/kernel/subsys_addr.h"

const char *rscfl_subsys_name[] = {
    SUBSYS_TABLE(SUBSYS_AS_STR_ARRAY)
};

int probes_init(void)
{
  kprobe_opcode_t **probe_addr;
  int rcd = 0, rcc = 0, rcp = 0, rckp = 0;
  kprobe_pre_handler_t pre_handler = pre_handler;
  int subsys_num;
  kprobe_opcode_t **probe_addrs_temp[] = {
    BLOCKLAYER_ADDRS,
    NETWORKINGIPV4IPV6_ADDRS,
    NETWORKINGGENERAL_ADDRS,
    EXT4FILESYSTEM_ADDRS,
    FILESYSTEMSVFSANDINFRASTRUCTURE_ADDRS,
    NETWORKINGDRIVERS_ADDRS,
    SECURITYSUBSYSTEM_ADDRS,
  };

  kretprobe_handler_t probe_pre_handlers_temp[] = {
    rscfl_pre_handler_BLOCKLAYER,
    rscfl_pre_handler_NETWORKINGIPV4IPV6,
    rscfl_pre_handler_NETWORKINGGENERAL,
    rscfl_pre_handler_EXT4FILESYSTEM,
    rscfl_pre_handler_FILESYSTEMSVFSANDINFRASTRUCTURE,
    rscfl_pre_handler_NETWORKINGDRIVERS,
    rscfl_pre_handler_SECURITYSUBSYSTEM,
  };

  kretprobe_handler_t probe_post_handlers_temp[] = {
    rscfl_rtn_handler_BLOCKLAYER,
    rscfl_rtn_handler_NETWORKINGIPV4IPV6,
    rscfl_rtn_handler_NETWORKINGGENERAL,
    rscfl_rtn_handler_EXT4FILESYSTEM,
    rscfl_rtn_handler_FILESYSTEMSVFSANDINFRASTRUCTURE,
    rscfl_rtn_handler_NETWORKINGDRIVERS,
    rscfl_rtn_handler_SECURITYSUBSYSTEM,
  };

  // stap disables preemption even when running begin/end probes.
  // however, _rscfl_shim_init and/or _netlink_setup might sleep,
  // causing an BUG: scheduled in atomic section kernel error (for
  // sleeping with preemption disabled).
  //
  // because we know what we're doing, temporarily enable
  // scheduling here.
  // TODO(lc525): see if we need to restore IRQs as well
  //
  rcc = _rscfl_cpus_init();
  preempt_enable();
  rcd = _rscfl_dev_init();
  rcp = rscfl_perf_init();
  rckp =
      rscfl_init_rtn_kprobes(probe_addrs_temp, sizeof(probe_pre_handlers_temp) /
                                                   sizeof(kretprobe_handler_t),
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
  int rcd = 0, rcc = 0 ;

  // see comment in probes_init for why we need to explicitly enable
  // preemption here
  preempt_enable();
  rcd = _rscfl_dev_cleanup();
  preempt_disable();
  rcc = _rscfl_cpus_cleanup();
  rscfl_unregister_kprobes();

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
static struct subsys_accounting *get_subsys(rscfl_subsys subsys_id)
{
  char x[] = "";

  if (should_acct()) {
    struct accounting *acct;
    struct subsys_accounting *subsys_acct;
    pid_acct *current_pid_acct;
    rscfl_shared_mem_layout_t *rscfl_mem;
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
      // our
      // memory.
      while (subsys_acct - rscfl_mem->subsyses <= ACCT_SUBSYS_NUM) {
        if (!subsys_acct->in_use) {
          // acct_subsys is an index that describes the offset from the start
          // of
          // subsyses as measured by number of struct subsys_accountings.
          // Recall that this is done as we need consistent indexing between
          // userspace and kernel space.
          subsys_offset = subsys_acct - rscfl_mem->subsyses;
          acct->acct_subsys[subsys_id] = subsys_offset;
          break;
        } else {
          subsys_acct++;
        }
      }
      if (subsys_offset == -1) {
        // We haven't found anywhere in the shared page where we can store
        // this subsystem.
        return NULL;
      }
      // Now need to initialise the subsystem's resources to be 0.
      subsys_acct = &rscfl_mem->subsyses[subsys_offset];
      memset(subsys_acct, 0, sizeof(struct subsys_accounting));
      subsys_acct->in_use = 1;

    } else {
      subsys_acct = &rscfl_mem->subsyses[subsys_offset];
    }
    return subsys_acct;
  }
  return NULL;
}

void rscfl_subsystem_entry(rscfl_subsys subsys_id, struct kretprobe_instance *probe)
{
  pid_acct *current_pid_acct;
  struct subsys_accounting *subsys_acct;
  preempt_disable();
  subsys_acct = get_subsys(subsys_id);
  current_pid_acct = CPU_VAR(current_acct);
  if (subsys_acct != NULL) {
    // We running acct_next on this syscall.
    BUG_ON(current_pid_acct == NULL); // As subsys_acct=>current_pid_acct.
    if (current_pid_acct->curr_subsys != subsys_id) {
      rscfl_subsys *prev_subsys;
      // We're new in this subsystem.
      current_pid_acct->probe_data->nest_level++;
        if (current_pid_acct->curr_subsys != -1) {
          struct subsys_accounting *prev_subsys_acct =
              get_subsys(current_pid_acct->curr_subsys);
          rscfl_perf_get_current_vals(prev_subsys_acct, 1);
        }
        //Start the couters for the subsystem we're entering.
        rscfl_perf_get_current_vals(subsys_acct, 0);
        // Update the subsystem tracking info.
        prev_subsys = (rscfl_subsys *) probe->data;
        *prev_subsys = current_pid_acct->curr_subsys;
        current_pid_acct->curr_subsys = subsys_id;
      
    }
  }
  preempt_enable();
}

void rscfl_subsystem_exit(rscfl_subsys subsys_id, struct kretprobe_instance *probe)
{
  pid_acct *current_pid_acct;
  preempt_disable();
  current_pid_acct = CPU_VAR(current_acct);
  if ((current_pid_acct != NULL) &&
      (current_pid_acct->probe_data->syscall_acct)) {
    // This syscall is being accounted for.
    struct subsys_accounting *subsys_acct = get_subsys(subsys_id);
    if (current_pid_acct->probe_data->nest_level) {
      // We are unrolling the nestings of subsystems, so we should do
      // accounting.
        rscfl_subsys *prev_subsys;
        // We don't get the perf values if we haven't left the netlink, and
        // gone into the real syscall.
        rscfl_perf_get_current_vals(subsys_acct, 1);

        // Start counters again for the subsystem we're returning back to.
        prev_subsys = (rscfl_subsys *) probe->data;
        if (*prev_subsys != -1) {
          struct subsys_accounting *prev_subsys_acct = get_subsys(*prev_subsys);
          rscfl_perf_get_current_vals(prev_subsys_acct, 0);

          // Update subsystem tracking data.
          current_pid_acct->curr_subsys = *prev_subsys;
        }
      }
      if (!--current_pid_acct->probe_data->nest_level) {
        // We have backed out of all nesting. This either means we have just
        // popped out of the netlink, in which case we are ready to account for
        // the real call. Otherweise, we have just finished the actual syscall
        // and should clear_acct_next.
        if (!current_pid_acct->probe_data->real_call) {
          _clear_acct_next(current->pid, -1);
        }
    }
  }
  preempt_enable();
}
