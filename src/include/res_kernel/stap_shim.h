#ifndef _RESOURCEFUL_H_
#define _RESOURCEFUL_H_

#include "config.h"

#include <linux/slab.h>
#include <linux/relay.h>
#include <linux/debugfs.h>

int _create_shared_mem(void);

int _fill_struct(long);

int _update_relay(void);

int acct_next(int);

#ifndef NDEBUG
  #define debugk(format, ...) printk(format, ##__VA_ARGS__)
#else
  #define debugk(format, ...)
#endif

#endif
