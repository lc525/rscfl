#ifndef _RES_API_H_
#define _RES_API_H_

#include <costs.h>

typedef char * rscfl_handle;

rscfl_handle rscfl_init();

int rscfl_acct_next();

int rscfl_read_acct(rscfl_handle handle, struct accounting *acct);

#endif
