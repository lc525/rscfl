#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>

#include <rscfl/kernel/probes.h>

#define RSCFL_DESC "rscfl: application-centric kernel resource measurement"

static int __init rscfl_init(void)
{
  int ret;
  ret = probes_init();
  if (ret) {
    printk(KERN_ERR, "rscfl: unable to initialise\n");
    return ret;
  } else {
    printk(KERN_NOTICE, "rscfl: running\n");
    return 0;
  }
}

static void __exit rscfl_cleanup(void)
{
  int ret;
  ret = probes_cleanup();
  if (ret) {
    printk(KERN_ERR, "rscfl: unable to cleanup\n");
  }
}

module_init(rscfl_init);
module_exit(rscfl_cleanup);

MODULE_LICENSE("Dual BSD/GPL");
MODULE_AUTHOR("Lucian Carata <lucian.carata@cl.cam.ac.uk>");
MODULE_AUTHOR("James Snee <james.snee@cl.cam.ac.uk>");
MODULE_AUTHOR("Oliver R. A. Chick <oliver.chick@cl.cam.ac.uk>");
MODULE_VERSION(RSCFL_VERSION);
MODULE_DESCRIPTION(RSCFL_DESC);
