/* Resourceful user-facing API and internal library data structures.
 * Usage example:
 *
 * init_resource_acct(); // called on each thread - does some memory allocation
 *                       // and calls an ioctl setting up resource accounting.
 * int filter = SYS | PROC | NET_SOCK; // defines what resources we're
 *                                     // interested in, default: ALL (include
 *                                     // all resources)
 * call_cost* cost_o = acct_next(filter);   // declares interest in measuring
 *the
 *                                          // resource consumption of the next
 *syscall
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

#include <netinet/tcp.h>

#include "rscfl/subsys_list.h"

#define BIT(x) 1U << x
#define ALL_BITS(x) (1U << (x + 1)) - 1
#define u32 unsigned int
#define u64 unsigned long long

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

/* acct_*** data structures.
 *
 * acct_CPU and acct_Memory are always present and thus do not have
 * corresponding entries in the resource enum.
 *
 *
 *
 */
struct acct_CPU
{
  u64 cycles;
  u64 branch_mispredictions;  // count
  u64 instructions;  // count
};

struct acct_Sys
{
};

struct acct_Proc
{
};

struct acct_Mem
{
  u64 alloc;
  u64 freed;
};

struct acct_Storage
{
  u64 avg_bandwidth;
  u64 io_wait;
  u64 seeks;
};

struct acct_Net
{
  struct tcp_info stats;
};

union accounting_component
{
  struct acct_Storage storage;
  struct acct_Net network;
};

struct subsys_accounting
{
  struct acct_CPU cpu;
  struct acct_Mem mem;
};

struct accounting
{
  struct subsys_accounting* acct_subsys[NUM_SUBSYSTEMS];
};

/* Main structure for storing per-call resource consumption data
 */
struct call_cost
{
  bool has_async;
  bool async_done;

  struct accounting sync;
  struct accounting async;
};

#ifdef STAGE_2
/* Accounting for global resource consumption that happens at the same
 * time as a system_call, but is not caused by it.
 *
 * Full interface TBD
 */
struct system_acct
{
  // struct system_quantum quanta[MAX_CONCURRENT_SYSCALLS];
  // int head_pos;
};

struct sys_cost
{
  // accounting for simultaneous workloads
  system_acct sys;        // do we want this per subsystem?
  int system_acct_start;  // do we track start/stop per subsystem - probably not
  int system_acct_stop;
};
#endif

/* Interface between user space and  kernel space for registering interest in
 * resource accounting events (added syscall)
 *
 * TODO(lc525) currently incomplete
 */
struct res_acct_cfg
{
  // should enable filters and customization of desired resource accounting

  // syscall_filter
  // resource_filter
};

// SYSCALL
int res_acct_open(struct res_acct_cfg* cfg);

#endif
