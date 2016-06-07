#ifndef _RSCFL_SUBSYS_H_
#define _RSCFL_SUBSYS_H_

#include "rscfl/costs.h"

int rscfl_subsys_entry(rscfl_subsys);

void rscfl_subsys_exit(rscfl_subsys);

int get_subsys(rscfl_subsys subsys_id,
               struct subsys_accounting **subsys_acct_ret);

#endif
