#ifndef _RSCFL_PROBES_H_
#define _RSCFL_PROBES_H_

#include "rscfl/costs.h"

#include "linux/kprobes.h"

int probes_init(void);
int probes_cleanup(void);

void rscfl_subsystem_entry(rscfl_subsys, struct kretprobe_instance *probe);

void rscfl_subsystem_exit(rscfl_subsys, struct kretprobe_instance *probe);

#endif
