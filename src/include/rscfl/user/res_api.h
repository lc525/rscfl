#ifndef _RES_API_H_
#define _RES_API_H_

#include "rscfl/costs.h"

typedef struct {
  char *buf;
  rscfl_syscall_id_t lst_syscall;
} * rscfl_handle;

rscfl_handle rscfl_init();

rscfl_handle rscfl_get_handle();

int rscfl_acct_next(rscfl_handle);

int rscfl_read_acct(rscfl_handle handle, struct accounting *acct);

#endif
