/**** Notice
 * probes.h: rscfl source code
 *
 * Copyright 2015-2017 The rscfl owners <lucian.carata@cl.cam.ac.uk>
 *
 * This file is part of the rscfl open-source project: github.com/lc525/rscfl;
 * Its licensing is governed by the LICENSE file at the root of the project.
 **/

#ifndef _RSCFL_PROBES_H_
#define _RSCFL_PROBES_H_

#include <trace/events/sched.h>

int probes_init(void);
void probes_free(void);
int probes_unregister(void);

// tracepoints for scheduler interposition
void get_tracepoints(struct tracepoint*, void*);
int register_sched_interposition(void);
int unregister_sched_interposition(void);

typedef enum {
  HAS_TRACEPOINT_NONE         = 8,
  HAS_TRACEPOINT_SCHED_SWITCH = 1,
  HAS_TRACEPOINT_MIGRATE_TASK = 2,
  HAS_TRACEPOINT_PROCESS_EXIT = 4,
  HAS_TRACEPOINT_ALL          = 7
} tracepoint_status_mask;

extern struct tracepoint *rscfl_sched_switch,
                         *rscfl_migrate_task,
                         *rscfl_process_exit;
extern short rscfl_tracepoint_status;

#endif
