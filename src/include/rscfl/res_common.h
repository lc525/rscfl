#ifndef _RES_COMMON_H_
#define _RES_COMMON_H_

#include "rscfl/config.h"

#define RSCFL_DATA_DRIVER "rscfl-data"
#define RSCFL_CTRL_DRIVER "rscfl-ctrl"

#define STRUCT_ACCT_NUM 136   // need to think about this
#define ACCT_SUBSYS_RATIO 5   // assume one syscall touches ~ 5 subsystems

#ifndef PAGE_SIZE
#define PAGE_SIZE 4096
#endif
#define PAGE_ROUND_UP(x) ( (((unsigned int)(x)) + PAGE_SIZE - 1)               \
                           & (~(PAGE_SIZE - 1)) )

#define PAIR_ALLOC_SIZE (sizeof(struct accounting)                             \
                         + ACCT_SUBSYS_RATIO * sizeof(struct subsys_accounting))
#define MMAP_BUF_SIZE PAGE_ROUND_UP(STRUCT_ACCT_NUM * PAIR_ALLOC_SIZE)

#define ACCT_SUBSYS_NUM ( (MMAP_BUF_SIZE                                       \
                           - STRUCT_ACCT_NUM * sizeof(struct accounting)       \
                          ) / sizeof(struct subsys_accounting) )

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
