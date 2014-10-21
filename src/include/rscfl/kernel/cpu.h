#ifndef _RSCFL_PERCPU_H_
#define _RSCFL_PERCPU_H_

#include "rscfl/kernel/hasht.h"

DECLARE_PER_CPU(u32, current_pid_ix);
DECLARE_PER_CPU(htbl, pid_acct_htbl);

int _rscfl_cpus_init(void);
int _rscfl_cpus_cleanup(void);

#endif
