/**** Notice
 * rscfl.c: rscfl source code
 *
 * Copyright 2015-2017 The rscfl owners <lucian.carata@cl.cam.ac.uk>
 *
 * This file is part of the rscfl open-source project: github.com/lc525/rscfl;
 * Its licensing is governed by the LICENSE file at the root of the project.
 **/

#include "rscfl/kernel/rscfl.h"

#include <linux/delay.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>

#include "rscfl/config.h"
#include "rscfl/kernel/cpu.h"
#include "rscfl/kernel/chardev.h"
#include "rscfl/kernel/kamprobes.h"
#include "rscfl/kernel/measurement.h"
#include "rscfl/kernel/priv_kallsyms.h"
#include "rscfl/kernel/probes.h"

int rscfl_is_stopped = 0;

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
  debugk(RDBG_INFO, "per-thread mmap alloc: Total: %lu, /acct: %d, /subsys: %lu\n",
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

  // Initialise scheduler interposition.
  for_each_kernel_tracepoint(get_tracepoints, NULL);
  if (rscfl_tracepoint_status != HAS_TRACEPOINT_ALL) {
    printk(KERN_ERR "rscfl: unable to find required kernel tracepoints\n");
    probes_unregister();
    return rscfl_tracepoint_status;
  }
  rc = register_sched_interposition();
  if (rc) {
    printk(KERN_ERR "rscfl: unable to interpose scheduler\n");
    probes_unregister();
    return rc;
  }
  printk(KERN_NOTICE "rscfl: running\n");
  return 0;


}

static void __exit rscfl_cleanup(void)
{
  int rcd = 0;
  rcd = _rscfl_dev_cleanup();
  kamprobes_free();

  if (rcd) {
    printk(KERN_ERR "rscfl: cannot cleanup rscfl drivers\n");
  }

}

// called by the RSCFL_SHUTDOWN_CMD IOCTL
void do_module_shutdown(void) {
  int rcs, rcc, rcp = 0;

  if(rscfl_is_stopped == 0) {
    rscfl_is_stopped = 1;
    rcs = unregister_sched_interposition();
    tracepoint_synchronize_unregister();
    rcc = _rscfl_cpus_cleanup();
    rcp = probes_unregister();
    debugk(RDBG_INFO, "probe cleanup completed\n");
    rscfl_counters_stop();

    if (rcs) {
      printk(KERN_ERR "rscfl: disabling scheduler interposition failed\n");
    }
    if (rcc) {
      printk(KERN_ERR "rscfl: cannot cleanup per-cpu hash tables\n");
    }
    if (rcp) {
      printk(KERN_ERR "rscfl: cannot cleanup probes\n");
    }
  }
}

module_init(rscfl_init);
module_exit(rscfl_cleanup);

MODULE_LICENSE("Dual BSD/GPL");
MODULE_AUTHOR("Lucian Carata <lucian.carata@cl.cam.ac.uk>");
MODULE_AUTHOR("Oliver R. A. Chick <oliver.chick@cl.cam.ac.uk>");
MODULE_AUTHOR("James Snee <james.snee@cl.cam.ac.uk>");
MODULE_VERSION(RSCFL_KVERSION);
MODULE_DESCRIPTION(RSCFL_DESC);
