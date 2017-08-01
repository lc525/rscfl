/**** Notice
 * acct.h: rscfl source code
 *
 * Copyright 2015-2017 The rscfl owners <lucian.carata@cl.cam.ac.uk>
 *
 * This file is part of the rscfl open-source project: github.com/lc525/rscfl;
 * Its licensing is governed by the LICENSE file at the root of the project.
 **/

#ifndef _KO_RESOURCEFUL_H_
#define _KO_RESOURCEFUL_H_

#include "rscfl/res_common.h"

struct rscfl_kernel_token
{
  short id;
  // holds current head of xen schedule out events list
  int val;
  // holds snapshot of xen sched_out value
  uint64_t val2;
  struct accounting* account;
};
typedef struct rscfl_kernel_token rscfl_kernel_token;

int update_acct(void);
int clear_acct_next(void);

#endif
