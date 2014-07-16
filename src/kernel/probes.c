#include "rscfl/kernel/stap_shim.h"
#include "rscfl/kernel/res_comm.h"

int probes_init(void)
{
  int rc1 = 0, rc2 = 0;

  // stap disables preemption even when running begin/end probes.
  // however, _rscfl_shim_init and/or _netlink_setup might sleep,
  // causing an BUG: scheduled in atomic section kernel error (for
  // sleeping with preemption disabled).
  //
  // because we know what we're doing, temporarily enable
  // scheduling here.
  // TODO(lc525): see if we need to restore IRQs as well
  //
  preempt_enable();
  rc1 = _rscfl_shim_init();
  rc2 = _netlink_setup();
  preempt_disable();
  if (rc1) {
    printk(KERN_ERR "rscfl: cannot initialize rscfl driver\n");
  }
  if (rc2) {
    printk(KERN_ERR "rscfl: cannot initialize netlink\n");
  }
  return (rc1 | rc2) ;
}

int probes_cleanup(void)
{
  int rc1 = 0, rc2 = 0;

  // see comment in probes_init for why we need to explicitly enable
  // preemption here
  preempt_enable();
  rc1 =_rscfl_shim_cleanup();
  rc2 = _netlink_teardown();
  preempt_disable();
  if (rc1) {
    printk(KERN_ERR "rscfl: cannot cleanup rscfl driver\n");
  }
  if (rc2) {
    printk(KERN_ERR "rscfl: cannot teardown netlink\n");
  }
  return (rc1 | rc2);
}
