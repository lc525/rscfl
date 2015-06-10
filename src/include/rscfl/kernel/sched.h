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
