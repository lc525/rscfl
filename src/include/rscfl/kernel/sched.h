#ifndef _RSCFL_SCHED_H_
#define _RSCFL_SCHED_H_

#include "rscfl/config.h"
#include "rscfl/costs.h"
#include "rscfl/kernel/hasht.h"

void on_ctx_switch(pid_t next_tid);
void on_cpu_switch(int cpu_from, int cpu_to, pid_t pid);
void on_task_exit(pid_t pid);


#endif
