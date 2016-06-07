#ifndef _RSCFL_MEASUREMENT_H_
#define _RSCFL_MEASUREMENT_H_

#include "rscfl/costs.h"

int rscfl_counters_init(void);

void rscfl_counters_stop(void);

int rscfl_counters_update_subsys_vals(struct subsys_accounting *add_subsys,
                                      struct subsys_accounting *minus_subsys);

#endif /* _MEASUREMENT_H_ */

