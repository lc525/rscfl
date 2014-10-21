#ifndef _RES_API_H_
#define _RES_API_H_

#include "rscfl/costs.h"
#include "rscfl/res_common.h"

#define NO_RELAY_ACCTS (MMAP_BUF_SIZE / sizeof(struct accounting))
#define MAX_PAYLOAD 1024 /* maximum payload size*/

typedef struct {
  char *buf;
  rscfl_syscall_id_t lst_syscall;
} * rscfl_handle;

rscfl_handle rscfl_init();

rscfl_handle rscfl_get_handle();

int rscfl_acct_next(rscfl_handle);

int rscfl_read_acct(rscfl_handle handle, struct accounting *acct);

/* slower version of read that merges (aggregates) current resource data into an
 * existing accounting data structure
 * */
int rscfl_merge_acct(rscfl_handle handle, struct accounting *acct);

#endif
