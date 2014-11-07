#ifndef _RSCFL_PERCPU_H_
#define _RSCFL_PERCPU_H_

#include "rscfl/config.h"
#include "rscfl/costs.h"
#include "rscfl/kernel/hasht.h"

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

//TODO(lc525): move kprobe_priv into one of the kprobe headers once jas250
//merges the kprobe API
//
// kprobe_priv stores probe counter snapshots so that one can determine
// the resource consumption between subsystem crossing boundaries.
//
// nest_level is incremented on probing nested functions. It is initially 0 and
// the application request for measuring the resources consumed by a system call
// is considered completed when nest_level returns to 0.
struct kprobe_priv {
  u32 nest_level;
  struct accounting *syscall_acct;
  long cycles;
  long wall_clock_time;
  _Bool real_call;
};
typedef struct kprobe_priv kprobe_priv;

struct pid_acct {
  struct hlist_node link; // item in the per-bucket linked list
  pid_t pid;
  struct rscfl_shared_mem_layout_t* shared_buf;        // shared with user-space
  struct kprobe_priv* probe_data;     // private data used by each kprobe
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

#endif
