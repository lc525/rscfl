/**** Notice
 * sched.h: rscfl source code
 *
 * Copyright 2015-2017 The rscfl owners <lucian.carata@cl.cam.ac.uk>
 *
 * This file is part of the rscfl open-source project: github.com/lc525/rscfl;
 * Its licensing is governed by the LICENSE file at the root of the project.
 **/

#ifndef _RSCFL_SCHED_H_
#define _RSCFL_SCHED_H_

#include "linux/sched.h"

void on_ctx_switch(void *ignore,
                   struct task_struct *prev,
                   struct task_struct *next);
void on_cpu_switch(void *ignore,
                   struct task_struct *p,
                   int cpu_to);
void on_task_exit(void *ignore, struct task_struct *p);


#endif
