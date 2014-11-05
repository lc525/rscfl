#ifndef _RSCFL_PERF_H_
#define _RSCFL_PERF_H_

#include "rscfl/costs.h"

int rscfl_perf_init(void);

int rscfl_perf_get_current_vals(struct subsys_accounting *acct_subsys,
				_Bool add);

#endif /* _PERF_H_ */
