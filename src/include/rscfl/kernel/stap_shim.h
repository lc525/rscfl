#ifndef _KO_RESOURCEFUL_H_
#define _KO_RESOURCEFUL_H_

#include "rscfl/costs.h"
#include "rscfl/res_common.h"


struct rscfl_shared_mem_layout_t
{
  struct accounting acct[STRUCT_ACCT_NUM];
  struct subsys_accounting subsyses[ACCT_SUBSYS_NUM];
};

typedef struct rscfl_shared_mem_layout_t rscfl_shared_mem_layout_t;

int should_acct(void);
int clear_acct_next(void);

#endif
