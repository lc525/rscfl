#include "rscfl/kernel/probes.h"

#include "linux/kprobes.h"

#include "rscfl/kernel/acct.h"
#include "rscfl/kernel/cpu.h"
#include "rscfl/kernel/kamprobes.h"
#include "rscfl/kernel/measurement.h"
#include "rscfl/kernel/priv_kallsyms.h"
#include "rscfl/kernel/shdw.h"
#include "rscfl/kernel/subsys_addr.h"

#define PROBE_LIST(_)               \
_(BLOCKLAYER)                       \
_(NETWORKINGIPV4IPV6)               \
_(NETWORKINGGENERAL)                \
_(FILESYSTEMSVFSANDINFRASTRUCTURE)  \
_(NETWORKINGDRIVERS)                \
_(SECURITYSUBSYSTEM)                \
_(XENHYPERVISORINTERFACE)           \
_(XENINTERRUPTS)

#define PROBES_AS_ADDRS(a) a##_ADDRS,
#define PROBES_AS_SYSCALL_TYPE(a) a##_INTERNAL_SYSCALL,
#define PROBES_AS_PRE_HANDLE(a) rscfl_pre_handler_##a,
#define PROBES_AS_RTN_HANDLE(a) rscfl_rtn_handler_##a,

int probes_init(void)
{
  kprobe_pre_handler_t pre_handler = pre_handler;
  u8 **probe_addrs_temp[] = {PROBE_LIST(PROBES_AS_ADDRS)};
  char *syscall_type_temp[] = {PROBE_LIST(PROBES_AS_SYSCALL_TYPE)};

  int i, rc, failures = 0;
  unsigned long flags;

  void (*probe_pre_handlers_temp[])(void) = {PROBE_LIST(PROBES_AS_PRE_HANDLE)};
  void (*probe_post_handlers_temp[])(void) = {PROBE_LIST(PROBES_AS_RTN_HANDLE)};

  int num_subsys = sizeof(probe_pre_handlers_temp) / sizeof(kretprobe_handler_t);

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
      }
      sub_addr++;
      j++;
    }
  }
  local_irq_restore(flags);
  return failures;
}

int probes_cleanup(void)
{
  kamprobes_unregister_all();
  return 0;
}
