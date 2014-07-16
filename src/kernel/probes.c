#include "rscfl/kernel/stap_shim.h"
#include "rscfl/kernel/res_comm.h"

int probes_init(void)
{
  int rc = (_rscfl_shim_init());
  if (rc) {
    return rc;
  }
  rc = _netlink_setup();
  if (rc) {
    return rc;
  }
  return 0;
}

int probes_cleanup(void)
{
  int rc =_rscfl_shim_cleanup();
  if (rc) {
    printk(KERN_ERR "rscfl: cannot cleanup rscfl driver\n");
    return rc;
  }
  rc = _netlink_teardown();
  if (rc) {
    printk(KERN_ERR "rscfl: cannot teardown netlink\n");
    return rc;
  }
  return 0;
}
