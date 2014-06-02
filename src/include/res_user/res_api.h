#ifndef _RES_API_H_
#define _RES_API_H_

#include <costs.h>

int rscfl_init(char **relay_f_data);

int rscfl_acct_next();

int rscfl_read_acct(char *relay_f_data, struct accounting *acct);

#endif
