/**** Notice
 * perf.h: rscfl source code
 *
 * Copyright 2015-2017 The rscfl owners <lucian.carata@cl.cam.ac.uk>
 *
 * This file is part of the rscfl open-source project: github.com/lc525/rscfl;
 * Its licensing is governed by the LICENSE file at the root of the project.
 **/

#ifndef _RSCFL_PERF_H_
#define _RSCFL_PERF_H_

#include "rscfl/costs.h"

int rscfl_perf_init(void);
void rscfl_perf_stop(void);
int rscfl_snapshot_perf(struct subsys_accounting *add_subsys,
                        struct subsys_accounting *minus_subsys);

#endif /* _PERF_H_ */
