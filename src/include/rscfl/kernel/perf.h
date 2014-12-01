#ifndef _RSCFL_PERF_H_
#define _RSCFL_PERF_H_

#include "rscfl/costs.h"

int rscfl_perf_init(void);
void rscfl_perf_stop(void);
int rscfl_snapshot_perf(struct subsys_accounting *add_subsys,
                        struct subsys_accounting *minus_subsys);

#endif /* _PERF_H_ */
