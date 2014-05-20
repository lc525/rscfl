#ifndef _RESOURCEFUL_H_
#define _RESOURCEFUL_H_

#include <linux/slab.h>
#include <linux/relay.h>
#include <linux/debugfs.h>

#define PROJNAME "resourceful"
#define SUBBUF_SIZE 262144
#define N_SUBBUFS 4

int _create_shared_mem(void);

int _fill_struct(long);

int _update_relay(void);

#endif
