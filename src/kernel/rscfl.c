#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>

#include <trace/events/sched.h>

#include <rscfl/config.h>
#include "rscfl/kernel/cpu.h"
#include "rscfl/kernel/chardev.h"
#include "rscfl/kernel/measurement.h"
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

  int rc;

  // Get addresses for private kernel symbols.
  rc = init_priv_kallsyms();
  if (rc) {
    printk(KERN_ERR "rscfl: cannot find required kernel kallsyms\n");
    return rc;
  }

  // Initialise per-CPU hash tables.
  rc = _rscfl_cpus_init();
  if (rc) {
    printk(KERN_ERR "rscfl: cannot initialize per-cpu hash tables\n");
    return rc;
  }
  debugk("per-thread mmap alloc: Total: %lu, /acct: %d, /subsys: %lu\n",
         MMAP_BUF_SIZE, STRUCT_ACCT_NUM, ACCT_SUBSYS_NUM);

  // Initialise the rscfl drivers.
  rc = _rscfl_dev_init();
  if (rc) {
    printk(KERN_ERR "rscfl: cannot initialize rscfl drivers\n");
    return rc;
  }

  // Initialise the rscfl counters, which measure the resources consumed.
  rc = rscfl_counters_init();
  if (rc) {
    printk(KERN_ERR "rscfl: cannot initialize rscfl measurement counters\n");
    return rc;
  }

  rc = probes_init();
  if (rc) {
    // Do not fail just because we couldn't set a couple of probes
    // instead, print a warning.
    printk(KERN_WARNING "rscfl: failed to insert %d probes\n", rc);
  }

  for_each_kernel_tracepoint(get_tracepoints, NULL);
  if (rscfl_tracepoint_status != HAS_TRACEPOINT_ALL) {
    printk(KERN_ERR "rscfl: unable to find required kernel tracepoints\n");
    probes_cleanup();
    return rscfl_tracepoint_status;
  }
  rc = register_sched_interposition();
  if (rc) {
    printk(KERN_ERR "rscfl: unable to interpose scheduler\n");
    probes_cleanup();
    return rc;
  }
  printk(KERN_NOTICE "rscfl: running\n");
  return 0;


}

static void __exit rscfl_cleanup(void)
{
  int rcs, rcd, rcc, rcp;

  rcs = unregister_sched_interposition();
  if (rcs) {
    printk(KERN_ERR "rscfl: disabling scheduler interposition failed\n");
  }

  rcd = _rscfl_dev_cleanup();
  rcp = probes_cleanup();
  rscfl_counters_stop();

  rcc = _rscfl_cpus_cleanup();

  if (rcd) {
    printk(KERN_ERR "rscfl: cannot cleanup rscfl drivers\n");
  }
  if (rcc) {
    printk(KERN_ERR "rscfl: cannot cleanup per-cpu hash tables\n");
  }
  if (rcp) {
    printk(KERN_ERR "rscfl: cannot cleanup probes\n");
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
