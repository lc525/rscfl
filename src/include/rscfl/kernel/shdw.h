#ifndef _RSCFL_SHDW_H_
#define _RSCFL_SDHW_H_

#include "rscfl/res_common.h"

int shdw_create(shdw_hdl *);

/*
 * Switch *all* pages to those in the shadow kernel.
 */
int shdw_switch(shdw_hdl);

/*
 * Switch the first n pages to be those of the shadow kernel.
 */
int shdw_switch_pages(shdw_hdl, int n);

/*
 * If executing a shadow kernel then return to the original kernel. Otherwise
 * has no effect.
 */
int shdw_reset(void);

int do_shdw_op(shdw_op, shdw_hdl *, int num_pages);

#endif
