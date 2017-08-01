/**** Notice
 * kamprobes.h: rscfl source code
 *
 * Copyright 2015-2017 The rscfl owners <lucian.carata@cl.cam.ac.uk>
 *
 * This file is part of the rscfl open-source project: github.com/lc525/rscfl;
 * Its licensing is governed by the LICENSE file at the root of the project.
 **/

#ifndef _RSCFL_KAMPROBES_H_
#define _RSCFL_KAMPROBES_H_

#include <linux/types.h>

int kamprobes_init(int max_probes);

void kamprobes_free(void);

void kamprobes_unregister_all(void);

int kamprobes_register(u8 **orig_addr, char sys_type, int (*pre_handler)(void),
                       void (*post_handler)(void));
#endif
