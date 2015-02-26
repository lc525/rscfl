#include "rscfl/kernel/probe_manager.h"

#include "rscfl/kernel/kamprobes.h"

void rscfl_unregister_probes(void)
{
  kamprobes_unregister_all();
}

int rscfl_probes_init(u8 **subsys_addrs[], char *sys_type[], int num_subsys,
                      int num_probes, void (*kp_pre_handler[])(void),
                      void (*kp_rtn_handler[])(void))
{
  int i;
  int rc, failures = 0;
  unsigned long flags;

  kamprobes_init(num_probes);
  local_irq_save(flags);
  for (i = 0; i < num_subsys; i++) {
    u8 **sub_addr = subsys_addrs[i];
    int j = 0;
    while (*sub_addr) {
      rc = kamprobes_register(sub_addr, sys_type[i][j],
                              kp_pre_handler[i], kp_rtn_handler[i]);
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
