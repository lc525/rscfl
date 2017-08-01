/**** Notice
 * shdw.h: rscfl source code
 *
 * Copyright 2015-2017 The rscfl owners <lucian.carata@cl.cam.ac.uk>
 *
 * This file is part of the rscfl open-source project: github.com/lc525/rscfl;
 * Its licensing is governed by the LICENSE file at the root of the project.
 **/

#ifndef _RSCFL_SHDW_H_
#define _RSCFL_SHDW_H_

#include "rscfl/res_common.h"

#if SHDW_ENABLED != 0

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

#endif /* SHDW_ENABLED */

#endif
