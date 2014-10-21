#ifndef _RSCFL_PERF_H_
#define _RSCFL_PERF_H_

#include "linux/perf_event.h"


int rscfl_perf_get_current_vals(struct accounting *);

int rscfl_perf_init(void);


#endif /* _PERF_H_ */
