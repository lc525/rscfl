#include "rscfl/kernel/probes.h"

#include "linux/kprobes.h"

#include "rscfl/kernel/chardev.h"
#include "rscfl/kernel/cpu.h"
#include "rscfl/kernel/kprobe_manager.h"
#include "rscfl/kernel/netlink.h"
#include "rscfl/kernel/perf.h"
#include "rscfl/kernel/subsys_addr.h"

int probes_init(void)
{
  kprobe_opcode_t **probe_addr;
  int rcd = 0, rcn = 0, rcc = 0, rcp = 0, rckp = 0;
  kprobe_pre_handler_t pre_handler = pre_handler;
  int subsys_num;
  kprobe_opcode_t **probe_addrs_temp[] = {
      NETWORKINGGENERAL_ADDRS, NETWORKINGGENERAL_ADDRS,
      FILESYSTEMSVFSANDINFRASTRUCTURE_ADDRS,
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
  rcn = _netlink_setup();
  rcp = rscfl_perf_init();
  preempt_disable();

  if (rcc) {
    printk(KERN_ERR "rscfl: cannot initialize per-cpu hash tables\n");
    return rcc;
  }
  if (rcd) {
    printk(KERN_ERR "rscfl: cannot initialize rscfl driver\n");
    return rcd;
  }
  if (rcn) {
    printk(KERN_ERR "rscfl: cannot initialize netlink\n");
    return rcn;
  }
  if (rcp) {
    printk(KERN_ERR "rscfl: cannot initialise perf\n");
    return rcp;
  }

  preempt_enable();
  rckp = rscfl_init_rtn_kprobes(probe_addrs_temp, 3, rscfl_default_pre_handler,
                                rscfl_default_rtn_handler);
  preempt_disable();
  if (rckp) {
    printk("rscfl: failed to create probes: %d\n", rckp);
  }
  // Need to make kprobes work properly, and then return rckp.
  return 0;
}

int probes_cleanup(void)
{
  int rcd = 0, rcn = 0, rcc = 0 ;

  // see comment in probes_init for why we need to explicitly enable
  // preemption here
  preempt_enable();
  rcn = _netlink_teardown();
  rcd = _rscfl_dev_cleanup();
  preempt_disable();
  rcc = _rscfl_cpus_cleanup();
  rscfl_unregister_kprobes();

  if (rcn) {
    printk(KERN_ERR "rscfl: cannot teardown netlink\n");
  }
  if (rcd) {
    printk(KERN_ERR "rscfl: cannot cleanup rscfl driver\n");
  }
  if (rcc) {
    printk(KERN_ERR "rscfl: cannot cleanup per-cpu hash tables\n");
  }
  return (rcn | rcd | rcc);
}
