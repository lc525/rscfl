/* Resourceful user-facing API and internal library data structures.
 * Usage example:
 *
 * init_resource_acct(); // called on each thread - does some memory allocation
 *                       // and calls an ioctl setting up resource accounting.
 * int filter = SYS | PROC | NET_SOCK; // defines what resources we're
 *                                     // interested in, default: ALL (include
 *                                     // all resources)
 * call_cost* cost_o = acct_next(filter);   // declares interest in measuring the
 *                                          // resource consumption of the next syscall
 * int fd = open("/../file_path", O_CREAT); // syscall being measured
 *
 * call_cost* cost_w = acct_next(filter);
 * int res = write(fd, &buf, BUF_SIZE);
 *
 * // if the write is asynchronous, cost_w will keep being updated by the
 * // kernel for a while
 *
 * // do whatever you want with the call_cost data. you can read the sync
 * // component as soon as the syscall is done, but you should touch the async
 * // component only when the kernel has set the async_done flag to true
 * // you can register a callback for when that happens with:
 * cost_callback_async(cost_w, callback_function);
 *
 * //...und so weiter
 *
 * fini_resource_acct(); // de-allocate resource accounting structures
 *
 */
#ifndef _SYSCALL_COST_H_
#define _SYSCALL_COST_H_

//#include<linux/tcp.h>

#ifdef __KERNEL__
#include <linux/types.h>
#else
#include <sys/types.h>
#endif

#ifndef RBIT
#define RBIT(x) 1U << x
#endif

#define ALL_BITS(x) (1U << (x + 1)) - 1
#define ru32 unsigned int
#define ru64 unsigned long long

#define RSCFL_ACCT_USE_BIT 0

#define STAGE_1
// #define STAGE_2  // The elements marked with #ifdef STAGE_2 will be
                    // implemented after all the STAGE_1 functionality is in place


/* The resource enum, together with the cost_bitmap structure, lets the end-user
 * quickly identify what kernel modules were touched when a syscall was made.
 * The corresponding acct_*** structures will be present in the accounting data
 * structure.
 *
 * A logical OR of resource members can also be explicitly passed by the user
 * when registering interest in system call resource accounting, as a filter.
 */
typedef enum {
  SYS              = RBIT(0),
  SYS_DEV          = RBIT(1),
  SYS_HW           = RBIT(2),
  SYS_IO           = RBIT(3),

  PROC             = RBIT(4),
  PROC_THR         = RBIT(5),
  PROC_SYNC        = RBIT(6),
  PROC_SCHED       = RBIT(7),
  PROC_IRQ         = RBIT(8),

  MEM              = RBIT(9),
  MEM_VIRT         = RBIT(10),
  MEM_MAP          = RBIT(11),
  MEM_PAGE         = RBIT(12),

  STORAGE          = RBIT(13),
  STORAGE_VFS      = RBIT(14),
  STORAGE_CACHE    = RBIT(15),
  STORAGE_FS       = RBIT(16),
  STORAGE_HW       = RBIT(17),

  NET              = RBIT(18),
  NET_SOCK         = RBIT(19),
  NET_PROTO        = RBIT(20),
  NET_HW           = RBIT(21),

  ALL              = ALL_BITS(21)
} resource;

typedef struct {
  unsigned long id;
  pid_t pid;
} rscfl_syscall_id_t;

struct cost_bitmap {
  ru32 primary; // logical OR between multiple resource elements
#ifdef STAGE_2
  ru64 ext;     // here for future extension
#endif
};



/* acct_*** data structures.
 *
 * acct_CPU and acct_Memory are always present and thus do not have
 * corresponding entries in the resource enum.
 *
 *
 *
 */
struct acct_CPU {
  ru64 cycles;
  ru64 branch_mispredictions; //count
  ru64 instructions; //count
  ru64 wall_clock_time;
};


struct acct_Sys {
};

struct acct_Proc {
};

struct acct_Mem {
  ru64 alloc;
  ru64 freed;
};

struct acct_Storage {
  ru64 avg_bandwidth;
  ru64 io_wait;
  ru64 seeks;
};

/*
struct acct_Net {
  //struct tcp_info stats;
};
*/

union accounting_component {
  struct acct_Storage storage;
  //struct acct_Net network;
};

struct accounting {
  struct cost_bitmap fields;   // logical OR of resource members

  struct acct_CPU cpu;
  struct acct_Mem mem;

  rscfl_syscall_id_t syscall_id;
  volatile long unsigned int in_use;

#ifdef STAGE_2
  accounting_component[3] kunit_acct;
  accounting_component* ext;
#endif
};

/* Main structure for storing per-call resource consumption data
 */
struct call_cost {

  char flags;
// has_async;
//  bool async_done;

  struct accounting sync;
  struct accounting async;
};



#ifdef STAGE_2
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
#endif



/* Interface between user space and  kernel space for registering interest in
 * resource accounting events (added syscall)
 *
 * TODO(lc525) currently incomplete
 */
struct res_acct_cfg {
  // should enable filters and customization of desired resource accounting

  // syscall_filter
  // resource_filter
};

//SYSCALL
int res_acct_open(struct res_acct_cfg* cfg);

#endif
