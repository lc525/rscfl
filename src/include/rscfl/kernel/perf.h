#ifndef _RSCFL_PERF_H_
#define _RSCFL_PERF_H_

#include "rscfl/costs.h"

int rscfl_perf_init(void);

int rscfl_perf_update_subsys_vals(struct subsys_accounting *add_subsys,
                                  struct subsys_accounting *minus_subsys);

#endif /* _PERF_H_ */
