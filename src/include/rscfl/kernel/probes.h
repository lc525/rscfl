#ifndef _RSCFL_PROBES_H_
#define _RSCFL_PROBES_H_

#include "linux/kprobes.h"

#include "rscfl/costs.h"

int probes_init(void);
int probes_cleanup(void);

void rscfl_subsystem_entry(rscfl_subsys);

void rscfl_subsystem_exit(rscfl_subsys);

#endif
