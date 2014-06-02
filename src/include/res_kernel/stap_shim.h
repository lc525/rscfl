#ifndef _KO_RESOURCEFUL_H_
#define _KO_RESOURCEFUL_H_

#include "config.h"
#include "costs.h"

#include <linux/slab.h>
#include <linux/relay.h>
#include <linux/debugfs.h>

struct free_accounting_pool {
  struct accounting *item;
  struct free_accounting_pool *next;
};

int _create_shared_mem(void);

int _fill_struct(long, struct accounting *);

int _update_relay(struct accounting *);

struct accounting * _should_acct(pid_t pid, int syscall_nr);

int acct_next(pid_t, int);

int _rscfl_shim_init(void);

int _clean_debugfs(void);

int _clear_acct_next(pid_t, int);

#ifndef NDEBUG
  #define debugk(format, ...) printk(format, ##__VA_ARGS__)
#else
  #define debugk(format, ...)
#endif

#endif
