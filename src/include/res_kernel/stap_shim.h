#ifndef _KO_RESOURCEFUL_H_
#define _KO_RESOURCEFUL_H_

#include "config.h"

#include <linux/slab.h>
#include <linux/relay.h>
#include <linux/debugfs.h>

int _create_shared_mem(void);

int _fill_struct(long);

int _update_relay(void);

int _should_acct(int pid, int syscall_nr);

int acct_next(pid_t, int);

int _clean_debugfs(void);

int _clear_acct_next(pid_t, int);

#ifndef NDEBUG
  #define debugk(format, ...) printk(format, ##__VA_ARGS__)
#else
  #define debugk(format, ...)
#endif

#endif
