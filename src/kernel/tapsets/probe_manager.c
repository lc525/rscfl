#include "rscfl/kernel/probe_manager.h"

#include "rscfl/kernel/kamprobes.h"

void rscfl_unregister_probes(void)
{
  kamprobes_unregister_all();
}

int rscfl_init_rtn_probes(u8 **subsys_addrs[], int num_subsys,
                          int num_probes, void (*kp_pre_handler[])(void),
                          void (*kp_rtn_handler[])(void))
{
  int i;
  int rc;
  unsigned long flags;

  kamprobes_init(num_probes);
  local_irq_save(flags);
  for (i = 0; i < num_subsys; i++) {
    u8 **sub_addr = subsys_addrs[i];
    while (*sub_addr) {
      rc = kamprobes_register(sub_addr, kp_pre_handler[i], kp_rtn_handler[i]);
      if (rc) {
        return rc;
      }
      sub_addr++;
    }
  }
  local_irq_restore(flags);
  return 0;
}
