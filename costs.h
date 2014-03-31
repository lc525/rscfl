#ifndef _SYSCALL_COST_H_
#define _SYSCALL_COST_H_

#include <netinet/tcp.h>

#define BIT(x) 1U << x
#define u32 unsigned int
#define u64 unsigned long long

/* Resourceful user-facing interface and data structures.
 *
 */

enum primary_fields {
  SYS              = BIT(0),
  SYS_DEV          = BIT(1),
  SYS_HW           = BIT(2),
  SYS_IO           = BIT(3),

  PROC             = BIT(4),
  PROC_THR         = BIT(5),
  PROC_SYNC        = BIT(6),
  PROC_SCHED       = BIT(7),
  PROC_IRQ         = BIT(8),

  MEM              = BIT(9),
  MEM_VIRT         = BIT(10),
  MEM_MAP          = BIT(11),
  MEM_PAGE         = BIT(12),

  STORAGE          = BIT(13),
  STORAGE_VFS      = BIT(14),
  STORAGE_CACHE    = BIT(15),
  STORAGE_FS       = BIT(16),
  STORAGE_HW       = BIT(17),

  NET              = BIT(18),
  NET_SOCK         = BIT(19),
  NET_PROTO        = BIT(20),
  NET_HW           = BIT(21)
};


struct cost_fields {
  u32 primary;
  u64 ext;
};



/* shared memory buffer */
// 1st page - header
struct call_cost_header;

struct CPU {
  u64 cycles;
  u64 branch_mispredictions; //count
  u64 instructions; //count
};

struct Memory {
  u64 alloc;
  u64 freed;
};

struct acct_storage {
  u64 avg_bandwidth;
  u64 io_wait;
  u64 seeks;
};

struct acct_network {
  tcp_info stats;
};

struct syscall_acct {
  cost_fields fields;
  CPU cpu;
  Memory mem;
  struct acct_storage; /* if fields & STORAGE == STORAGE */
  struct acct_network; /* if fields & NETWORK == NETWORK */
};

/* in ring buffer starting from second page */
struct call_cost {
  u64 call_id;
  bool has_async;

  syscall_acct sync;
  syscall_acct async;
};



/* Accounting for global resource consumption that happens at the same
 * time as a system_call, but is not caused by it.
 *
 * Full interface TBD
 */
struct system_acct {
  //struct system_quantum quanta[MAX_CONCURRENT_SYSCALLS];
  //int head_pos;
};

struct sys_cost {
  // accounting for simultaneous workloads
  system_acct sys; // do we want this per subsystem?
  int system_acct_start; // do we track start/stop per subsystem - probably not
  int system_acct_stop;
};



/* Interface between user space and  kernel space for registering interest in
 * resource accounting events (added syscall)
 *
 * TODO(lc525) currently incomplete
 */
struct res_acct_cfg {
  // should enable filters and customization of desired resource accounting

  // syscall_filter
  // resource_filter
}

//SYSCALL
int res_acct_open(res_acct_cfg* cfg);

#endif
