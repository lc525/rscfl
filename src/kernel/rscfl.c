#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>

#include <trace/events/sched.h>

#include <rscfl/config.h>
#include <rscfl/kernel/probes.h>
#include <rscfl/kernel/sched.h>

static int __init rscfl_init(void)
{
  int ret, retsch;
  ret = probes_init();
  if (ret) {
    printk(KERN_ERR, "rscfl: unable to initialise\n");
    return ret;
  } else {
    retsch = register_sched_interposition();
    if(retsch) {
      printk(KERN_ERR, "rscfl: unable to interpose scheduler\n");
      return retsch;
    } else {
      printk(KERN_NOTICE, "rscfl: running\n");
      return 0;
    }
  }
}

static void __exit rscfl_cleanup(void)
{
  int ret, retsch;
  retsch = unregister_sched_interposition();
  if (retsch) {
    printk(KERN_ERR, "rscfl: disabling scheduler interposition failed\n");
  }
  ret = probes_cleanup();
  if (ret) {
    printk(KERN_ERR, "rscfl: unable to cleanup\n");
  }
  tracepoint_synchronize_unregister();
}


int register_sched_interposition() {
  WARN_ON(register_trace_sched_switch(on_ctx_switch, NULL));
  WARN_ON(register_trace_sched_migrate_task(on_cpu_switch, NULL));
  WARN_ON(register_trace_sched_process_exit(on_task_exit, NULL));
  return 0;
}

int unregister_sched_interposition() {
  unregister_trace_sched_switch(on_ctx_switch, NULL);
  unregister_trace_sched_migrate_task(on_cpu_switch, NULL);
  unregister_trace_sched_process_exit(on_task_exit, NULL);
  return 0;
}

module_init(rscfl_init);
module_exit(rscfl_cleanup);

MODULE_LICENSE("Dual BSD/GPL");
MODULE_AUTHOR("Lucian Carata <lucian.carata@cl.cam.ac.uk>");
MODULE_AUTHOR("Oliver R. A. Chick <oliver.chick@cl.cam.ac.uk>");
MODULE_AUTHOR("James Snee <james.snee@cl.cam.ac.uk>");
MODULE_VERSION(RSCFL_VERSION);
MODULE_DESCRIPTION(RSCFL_DESC);
