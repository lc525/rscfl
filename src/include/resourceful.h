#ifndef _RESOURCEFUL_H_
#define _RESOURCEFUL_H_

#include "config.h"

#include <linux/slab.h>
#include <linux/relay.h>
#include <linux/debugfs.h>

int _create_shared_mem(void);

int _fill_struct(long);

int _update_relay(void);

#endif
