/**** Notice
 * cpu.h: rscfl source code
 *
 * Copyright 2015-2017 The rscfl owners <lucian.carata@cl.cam.ac.uk>
 *
 * This file is part of the rscfl open-source project: github.com/lc525/rscfl;
 * Its licensing is governed by the LICENSE file at the root of the project.
 **/

#ifndef _RSCFL_PERCPU_H_
#define _RSCFL_PERCPU_H_

#include "rscfl/config.h"
#include "rscfl/costs.h"
#include "rscfl/kernel/hasht.h"
#include "rscfl/kernel/priv_kallsyms.h"
#include "rscfl/res_common.h"
#include "rscfl/subsys_list.h"

#define SUBSYS_STACK_HEIGHT 20

/* Per-CPU (pid -> accounting buf) hash table pid_to_acct_tbl
 *
 * We allocate one hash table per CPU in order to hold (pid, accounting*) pairs
 * for processes (pids) that do resource accounting and are active (or have
 * previously run) on that CPU but have not terminated.
 *
 * The key of the per-cpu hashtable is a process pid (pid_t)
 * The value of the per-cpu hashtable is a struct pid_acct
 *
 * The hashtable is maintained accurate by interposing scheduler actions
 * (i.e when a process migrates from one cpu to another, table entries
 * are also copied across corresponding cpu hash tables).
 *
 * As scheduler operations are done with preemption disabled, we can typically
 * access the per-cpu hash tables directly using the CPU_TBL macro defined in
 * hasht.h
 *
 * INSERTION example:
 *
 *   pid_acct* pa = (pid_acct *)kmalloc(sizeof(pid_acct), GFP_ATOMIC);
 *   pa->pid = current->pid;
 *   hash_add(CPU_TBL(pid_acct_tbl), &pa->link, pa->pid);
 *
 * LOOKUP example (iterate through all elements in a hash bucket):
 *
 *    pid_acct *it;
 *    hash_for_each_possible(CPU_TBL(pid_acct_tbl), it, link, key)
 *      if(it->pid == key)
 *        //do stuff with pid_acct *it
 */

// kprobe_priv stores probe counter snapshots so that one can determine
// the resource consumption between subsystem crossing boundaries.
struct probe_priv {
  struct accounting *syscall_acct;
  long cycles;
  long wall_clock_time;
};
typedef struct probe_priv probe_priv;

struct pid_acct {
  struct hlist_node link; // item in the per-bucket linked list
  pid_t pid;
  struct rscfl_acct_layout_t *shared_buf;        // shared with user-space
  probe_priv *probe_data;     // private data used by each probe
  rscfl_ctrl_layout_t *ctrl;  // pointer to the mapped data in the control driver.
  rscfl_subsys subsys_stack[SUBSYS_STACK_HEIGHT];
  rscfl_subsys *subsys_ptr;
  _Bool executing_probe;
  struct rscfl_kernel_token *default_token;
//  struct rscfl_kernel_token *null_token;
  struct rscfl_kernel_token *token_ix[MAX_TOKENS];
  volatile struct rscfl_kernel_token *active_token;
  unsigned short num_tokens;
  unsigned short next_ctrl_token;
  int shdw_kernel;
  int shdw_pages;
};
typedef struct pid_acct pid_acct;

DECLARE_PER_CPU_HASHTABLE(pid_acct_tbl, CPU_PIDACCT_HTBL_LOGSIZE);

/* current_acct
 * This variable always contains a pointer to the pid_acct structure in
 * use for the pid currently running on the CPU.
 *
 * Used for avoiding unnecessary hashtable lookups.
 *
 * Updated on scheduler actions.
 */
DECLARE_PER_CPU(pid_acct*, current_acct);


/*
 * Per-CPU initialization and cleanup. Run these with preemption disabled,
 * as we don't want to be moved around while initializing per-cpu structures.
 */
int _rscfl_cpus_init(void);
int _rscfl_cpus_cleanup(void);

static inline int is_vm(void) {
  return *KPRIV(HYPERVISOR_shared_info) != KPRIV(xen_dummy_shared_info);
}

#endif
