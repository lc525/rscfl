#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>

#include <trace/events/sched.h>

#include <rscfl/config.h>
#include <rscfl/kernel/priv_kallsyms.h>
#include <rscfl/kernel/probes.h>
#include <rscfl/kernel/sched.h>

void get_tracepoints(struct tracepoint*, void*);
int register_sched_interposition(void);
int unregister_sched_interposition(void);

typedef enum {
  HAS_TRACEPOINT_NONE         = 8,
  HAS_TRACEPOINT_SCHED_SWITCH = 1,
  HAS_TRACEPOINT_MIGRATE_TASK = 2,
  HAS_TRACEPOINT_PROCESS_EXIT = 4,
  HAS_TRACEPOINT_ALL          = 7
} tracepoint_status_mask;

static struct tracepoint *rscfl_sched_switch,
                         *rscfl_migrate_task,
                         *rscfl_process_exit;
static short rscfl_tracepoint_status = HAS_TRACEPOINT_NONE;

static int __init rscfl_init(void)
{
  int ret, retsch;
  ret = probes_init();
  if (ret) {
    printk(KERN_ERR "rscfl: unable to initialise\n");
    return ret;
  } else {
    for_each_kernel_tracepoint(get_tracepoints, NULL);
    if(rscfl_tracepoint_status != HAS_TRACEPOINT_ALL) {
      printk(KERN_ERR "rscfl: unable to find required kernel tracepoints");
      return rscfl_tracepoint_status;
    }
    retsch = register_sched_interposition();
    if(retsch) {
      printk(KERN_ERR "rscfl: unable to interpose scheduler\n");
      return retsch;
    } else {
      printk(KERN_NOTICE "rscfl: running\n");
      return 0;
    }
  }
}

static void __exit rscfl_cleanup(void)
{
  int ret, retsch;
  retsch = unregister_sched_interposition();
  if (retsch) {
    printk(KERN_ERR "rscfl: disabling scheduler interposition failed\n");
  }
  ret = probes_cleanup();
  if (ret) {
    printk(KERN_ERR "rscfl: unable to cleanup\n");
  }
  tracepoint_synchronize_unregister();
}


void get_tracepoints(struct tracepoint *tp, void *ignore)
{
  if(rscfl_tracepoint_status == HAS_TRACEPOINT_ALL) return;
  if(strcmp(tp->name, "sched_switch") == 0) {
    rscfl_sched_switch = tp;
    rscfl_tracepoint_status &= HAS_TRACEPOINT_ALL; // clear "NONE" bit
    rscfl_tracepoint_status |= HAS_TRACEPOINT_SCHED_SWITCH;
    return;
  }
  if(strcmp(tp->name, "sched_migrate_task") == 0) {
    rscfl_migrate_task = tp;
    rscfl_tracepoint_status &= HAS_TRACEPOINT_ALL; // clear "NONE" bit
    rscfl_tracepoint_status |= HAS_TRACEPOINT_MIGRATE_TASK;
    return;
  }
  if(strcmp(tp->name, "sched_process_exit") == 0) {
    rscfl_process_exit = tp;
    rscfl_tracepoint_status &= HAS_TRACEPOINT_ALL; // clear "NONE" bit
    rscfl_tracepoint_status |= HAS_TRACEPOINT_PROCESS_EXIT;
    return;
  }
}


int register_sched_interposition()
{
  WARN_ON(tracepoint_probe_register(rscfl_sched_switch, on_ctx_switch, NULL));
  WARN_ON(tracepoint_probe_register(rscfl_migrate_task, on_cpu_switch, NULL));
  WARN_ON(tracepoint_probe_register(rscfl_process_exit, on_task_exit, NULL));
  return 0;
}

int unregister_sched_interposition()
{
  WARN_ON(tracepoint_probe_unregister(rscfl_sched_switch, on_ctx_switch, NULL));
  WARN_ON(tracepoint_probe_unregister(rscfl_migrate_task, on_cpu_switch, NULL));
  WARN_ON(tracepoint_probe_unregister(rscfl_process_exit, on_task_exit, NULL));
  return 0;
}

module_init(rscfl_init);
module_exit(rscfl_cleanup);

MODULE_LICENSE("Dual BSD/GPL");
MODULE_AUTHOR("Lucian Carata <lucian.carata@cl.cam.ac.uk>");
MODULE_AUTHOR("Oliver R. A. Chick <oliver.chick@cl.cam.ac.uk>");
MODULE_AUTHOR("James Snee <james.snee@cl.cam.ac.uk>");
MODULE_VERSION(RSCFL_KVERSION);
MODULE_DESCRIPTION(RSCFL_DESC);
