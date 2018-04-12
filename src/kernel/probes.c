/**** Notice
 * probes.c: rscfl source code
 *
 * Copyright 2015-2017 The rscfl owners <lucian.carata@cl.cam.ac.uk>
 *
 * This file is part of the rscfl open-source project: github.com/lc525/rscfl;
 * Its licensing is governed by the LICENSE file at the root of the project.
 **/

#include "rscfl/kernel/probes.h"

#include "rscfl/kernel/acct.h"
#include "rscfl/kernel/cpu.h"
#include "rscfl/kernel/kamprobes.h"
#include "rscfl/kernel/measurement.h"
#include "rscfl/kernel/priv_kallsyms.h"
#include "rscfl/kernel/sched.h"
#include "rscfl/kernel/shdw.h"
#include "rscfl/kernel/subsys_addr.h"

#define PROBE_LIST(_)               \
_(BLOCKLAYER)                       \
_(NETWORKINGGENERAL)                \
_(NETWORKINGDRIVERS)                \
_(NETWORKINGIPV4IPV6)               \
_(FILESYSTEMSVFSANDINFRASTRUCTURE)  \
_(SECURITYSUBSYSTEM)                \
_(XENHYPERVISORINTERFACE)           \
_(XENINTERRUPTS)


#define PROBES_AS_ADDRS(a) a##_ADDRS,
#define PROBES_AS_SYSCALL_TYPE(a) a##_INTERNAL_SYSCALL,
#define PROBES_AS_PRE_HANDLE(a) rscfl_pre_handler_##a,
#define PROBES_AS_RTN_HANDLE(a) rscfl_rtn_handler_##a,

int probes_init(void)
{
  u8 **probe_addrs_temp[] = {PROBE_LIST(PROBES_AS_ADDRS)};
  char *syscall_type_temp[] = {PROBE_LIST(PROBES_AS_SYSCALL_TYPE)};

  int i, rc, failures = 0, probes = 0;
  unsigned long flags;

  int (*probe_pre_handlers_temp[])(void) = {PROBE_LIST(PROBES_AS_PRE_HANDLE)};
  void (*probe_post_handlers_temp[])(void) = {PROBE_LIST(PROBES_AS_RTN_HANDLE)};

  int num_subsys = sizeof(probe_pre_handlers_temp) / sizeof(u8*);

  kamprobes_init(RSCFL_NUM_PROBES);
  local_irq_save(flags);
  for (i = 0; i < num_subsys; i++) {
    u8 **sub_addr = probe_addrs_temp[i];
    int j = 0;
    while (*sub_addr) {
      rc = kamprobes_register(sub_addr, syscall_type_temp[i][j],
                              probe_pre_handlers_temp[i],
                              probe_post_handlers_temp[i]);
      if (rc) {
        failures++;
      } else {
        probes++;
      }
      sub_addr++;
      j++;
    }
  }
  local_irq_restore(flags);
  debugk(RDBG_INFO, "Registered %d probes\n", probes - failures);
  return failures;
}

void probes_free() {
  kamprobes_free();
}

int probes_unregister(void)
{
  kamprobes_unregister_all();
  return 0;
}


// scheduler interposition probes
struct tracepoint *rscfl_sched_switch,
                  *rscfl_migrate_task,
                  *rscfl_process_exit;
short rscfl_tracepoint_status = HAS_TRACEPOINT_NONE;

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
