#ifndef _RES_API_H_
#define _RES_API_H_

#include <costs.h>

typedef struct {
  char *buf;
  rscfl_syscall_id_t lst_syscall;
} * rscfl_handle;

rscfl_handle rscfl_init();

int rscfl_acct_next(rscfl_handle);

int rscfl_read_acct(rscfl_handle handle, struct accounting *acct);

#endif
