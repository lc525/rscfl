#include "rscfl/kernel/probes.h"

#include "rscfl/kernel/chardev.h"
#include "rscfl/kernel/cpu.h"
#include "rscfl/kernel/netlink.h"

int probes_init(void)
{
  int rcd = 0, rcn = 0, rcc = 0;

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
  preempt_disable();

  if (rcc) {
    printk(KERN_ERR "rscfl: cannot initialize per-cpu hash tables\n");
  }
  if (rcd) {
    printk(KERN_ERR "rscfl: cannot initialize rscfl driver\n");
  }
  if (rcn) {
    printk(KERN_ERR "rscfl: cannot initialize netlink\n");
  }
  return (rcd | rcn | rcc);
}

int probes_cleanup(void)
{
  int rcd = 0, rcn = 0, rcc = 0;

  // see comment in probes_init for why we need to explicitly enable
  // preemption here
  preempt_enable();
  rcn = _netlink_teardown();
  rcd = _rscfl_dev_cleanup();
  preempt_disable();
  rcc = _rscfl_cpus_cleanup();

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
