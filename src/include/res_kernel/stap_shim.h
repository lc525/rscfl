#ifndef _KO_RESOURCEFUL_H_
#define _KO_RESOURCEFUL_H_

#include "config.h"
#include "costs.h"

#define FILL_MM 0
#define FILL_NET 1
#define FILL_FS 2
#define FILL_GLOBAL 3

int _create_shared_mem(void);

int _fill_struct(long, long, struct accounting *, long);

struct accounting * _should_acct(pid_t pid, int syscall_nr);

int acct_next(pid_t, int);

int _rscfl_shim_init(void);

int _rscfl_shim_cleanup(void);

int _clear_acct_next(pid_t, int);

#ifndef NDEBUG
  #define debugk(format, ...) printk(format, ##__VA_ARGS__)
#else
  #define debugk(format, ...)
#endif

#endif
