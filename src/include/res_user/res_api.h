#ifndef _RES_API_H_
#define _RES_API_H_

int rscfl_init() __attribute__ ((constructor));

int rscfl_acct_next();

int rscfl_acct();

void rscfl_atexit_handler();

#endif
