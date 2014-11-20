#ifndef _RES_COMMON_H_
#define _RES_COMMON_H_

#include "rscfl/config.h"

#define RSCFL_DATA_DRIVER "rscfl-data"
#define RSCFL_CTRL_DRIVER "rscfl-ctrl"
#define MMAP_BUF_SIZE 81920  // need to think about this
#define STRUCT_ACCT_NUM 10   // Need to think about this.
#define ACCT_SUBSYS_RATIO 5  // Do we think that each syscall touches 5
                             // subsystems?
#define ACCT_SUBSYS_NUM STRUCT_ACCT_NUM* ACCT_SUBSYS_RATIO

#ifndef NDEBUG
#define debugk(format, ...) printk(format, ##__VA_ARGS__)
#else
#define debugk(format, ...)
#endif

struct syscall_interest_t
{
  unsigned long syscall_id;
  int syscall_nr;
};
typedef struct syscall_interest_t syscall_interest_t;

#endif
