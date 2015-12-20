#ifndef _KO_RESOURCEFUL_H_
#define _KO_RESOURCEFUL_H_

#include "rscfl/res_common.h"

struct rscfl_kernel_token
{
  short id;
  int val;
  struct accounting* account;
};
typedef struct rscfl_kernel_token rscfl_kernel_token;

int should_acct(void);
int clear_acct_next(void);

#endif
