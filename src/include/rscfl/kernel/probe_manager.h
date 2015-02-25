#ifndef _RSCFL_PROBE_MANAGER_H_
#define _RSCFL_PROBE_MANAGER_H_

#include <linux/types.h>

int kamprobes_init(int);

void rscfl_unregister_probes(void);

// Create and init multiple kprobes
int rscfl_init_rtn_probes(u8 **subsys_addrs[], char *sys_type[], int num_subsys,
                          int num_probes, void (*pre_handler[])(void),
                          void (*rtn_handler[])(void));

#endif
