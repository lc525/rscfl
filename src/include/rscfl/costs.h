/* Resourceful measurement (per-subsystem cost) data structures
 */
#ifndef _SYSCALL_COST_H_
#define _SYSCALL_COST_H_

#ifdef __KERNEL__
  #include <linux/limits.h>
  #include <linux/tcp.h>
  #include <linux/time.h>
  #include <linux/types.h>
#else
  #include <limits.h>
  #include <stdlib.h>
  #include <time.h>
  #include <sys/types.h>
  #include <netinet/tcp.h>
#endif

#include "rscfl/subsys_list.h"

#ifdef __cplusplus
  #define _Bool bool
#endif
#ifndef ru32
  #define ru32 unsigned int
#endif
#ifndef ru64
  #define ru64 unsigned long long
#endif

#define RSCFL_ACCT_USE_BIT 0

/* The resource enum, together with the cost_bitmap structure, lets the end-user
 * quickly identify what kernel modules were touched when a syscall was made.
 * The corresponding acct_*** structures will be present in the accounting data
 * structure.
 *
 * A logical OR of resource members can also be explicitly passed by the user
 * when registering interest in system call resource accounting, as a filter.
 */

typedef struct
{
  unsigned long id;
  pid_t pid;
} rscfl_syscall_id_t;

/* acct_*** data structures.
 *
 * acct_CPU and acct_Mem are always present and thus do not have
 * corresponding entries in the resource enum.
 *
 */
struct acct_CPU
{
  ru64 cycles;
  ru64 branch_mispredictions; //count
  ru64 instructions; //count
  ru64 alignment_faults;
  struct timespec wall_clock_time;
};

struct acct_Mem
{
  ru64 alloc;
  ru64 freed;
  ru64 page_faults;
  ru64 align_faults;
};

struct acct_Storage
{
  ru64 avg_bandwidth;
  ru64 io_wait;
  ru64 seeks;
};

struct acct_Net
{
  struct tcp_info stats;
};

struct acct_Sched
{
  struct timespec wct_out_local;
  ru64 cycles_out_local;
  struct timespec wct_out_hyp;
  ru64 hypervisor_schedules;
  ru64 hypervisor_cycles;
  ru64 hypervisor_evtchn_pending_size;
  int hypervisor_credits_min;
  int hypervisor_credits_max;
};

struct subsys_accounting
{
  struct acct_CPU cpu;
  struct acct_Mem mem;
  struct acct_Sched sched;
  // The number of times another subsystem called into this subsystem.
  ru64 subsys_entries;
  // The number of times this subsystem called into another subsystem.
  ru64 subsys_exits;
  volatile _Bool in_use;
};

struct accounting
{
  volatile _Bool in_use;
  /*
   * Used as the interface for the kernel code to return an error to the
   * userspace.
   */
  volatile int rc;
  rscfl_syscall_id_t syscall_id;
  // Indexes into offsets from the start of the subsys section of
  // rscfl_pid_page->buf.
  short acct_subsys[NUM_SUBSYSTEMS];
  short nr_subsystems;
};

#endif /*_SYSCALL_COST_H_*/
