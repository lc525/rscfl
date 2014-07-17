#ifndef _KO_RESOURCEFUL_H_
#define _KO_RESOURCEFUL_H_

#include "rscfl/config.h"
#include "rscfl/costs.h"

#define FILL_MM 0
#define FILL_NET 1
#define FILL_FS 2
#define FILL_GLOBAL 3

int _create_shared_mem(void);

int _fill_struct(long, long, struct accounting *, long);

int _should_acct(pid_t pid, int syscall_nr, struct accounting**, int probe_nest, const char* name);

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
