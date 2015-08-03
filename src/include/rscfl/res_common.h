#ifndef _RES_COMMON_H_
#define _RES_COMMON_H_

#include <linux/ioctl.h>

#ifdef __KERNEL__
  #include <linux/time.h>
#else
  #include <time.h>
#endif

#include "rscfl/config.h"
#include "rscfl/costs.h"
#include "rscfl/macros.h"

#define RSCFL_DATA_DRIVER "rscfl-data"
#define RSCFL_CTRL_DRIVER "rscfl-ctrl"

/*
 * as a point of reference,
 *  STRUCT_ACCT_NUM 136
 *  ACCT_SUBSYS_RATIO 5
 *
 *  allocates 81920 bytes (20 pages) per rscfl handle (thread using rscfl)
 *  with space for 136 struct accounting
 *                 684 struct subsys_accounting
 *                 32 bytes wasted (aligning to PAGE_SIZE)
 *  the ratio is not fully respected in order to optimize memory usage
 *  (if more struct subsys_accounting fit because of the alignment, we'll
 *  allow those as well)
 *
 *  estimates above done for
 *    sizeof(struct accounting) = 240
 *    sizeof(struct subsys_accounting) = 72
 *
 */
#define STRUCT_ACCT_NUM 13
#define ACCT_SUBSYS_RATIO 5   // assume one syscall touches ~ 5 subsystems
#define NUM_READY_TOKENS 10   // Number of tokens that the kernel can prepare
                              // in advance.

#ifndef PAGE_SIZE
#define PAGE_SIZE 4096
#endif
#define PAGE_ROUND_UP(x) ( (((unsigned int)(x)) + PAGE_SIZE - 1)               \
                           & (~(PAGE_SIZE - 1)) )

#define PAIR_ALLOC_SIZE (sizeof(struct accounting)                             \
                         + ACCT_SUBSYS_RATIO * sizeof(struct subsys_accounting))
#define MMAP_BUF_SIZE PAGE_ROUND_UP(STRUCT_ACCT_NUM * PAIR_ALLOC_SIZE)
#define MMAP_CTL_SIZE PAGE_SIZE

#define ACCT_SUBSYS_NUM ( (MMAP_BUF_SIZE                                       \
                           - STRUCT_ACCT_NUM * sizeof(struct accounting)       \
                          ) / sizeof(struct subsys_accounting) )

#define RSCFL_SHDW_CMD _IOR('R', 0x2F, struct rscfl_ioctl)
#define RSCFL_SHUTDOWN_CMD _IO('R', 0x30)

/*
 * Shadow kernels.
 */
typedef enum {NOP, SPAWN_ONLY, SPAWN_SWAP_ON_SCHED, SWAP} shdw_op;
typedef int shdw_hdl;

struct rscfl_acct_layout_t
{
  struct accounting acct[STRUCT_ACCT_NUM];
  struct subsys_accounting subsyses[ACCT_SUBSYS_NUM];
};
typedef struct rscfl_acct_layout_t rscfl_acct_layout_t;

struct syscall_interest_t
{
  unsigned long syscall_id;
  int syscall_nr;
  int token;
  int tail_ix;
  shdw_hdl use_shdw;
  int shdw_pages;
  _Bool start_measurement;
};
typedef struct syscall_interest_t syscall_interest_t;

struct rscfl_ctrl_layout_t
{
  unsigned int version;
  syscall_interest_t interest;
  int new_tokens[NUM_READY_TOKENS];
  int num_new_tokens;
};
typedef struct rscfl_ctrl_layout_t rscfl_ctrl_layout_t;

struct rscfl_ioctl
{
  // in.
  shdw_op shdw_operation;
  // Handle to the shadow kernel to swap to if shdw_operation==SWAP.
  shdw_hdl swap_to_shdw;
  // When shdw_operation==SWAP then swap the first num_shdw_pages to the shadow
  // kernel.
  int num_shdw_pages;
  // out.
  shdw_hdl new_shdw_id;
};
typedef struct rscfl_ioctl rscfl_ioctl_t;

#ifdef __cplusplus
extern "C" {
#endif
ru64 rscfl_get_cycles(void);
void rscfl_timespec_add(struct timespec *to, const struct timespec *from);
void rscfl_timespec_add_ns(struct timespec *to, const ru64 from);

// per-component timespec diff
// this substracts each of the timespec components of ex from the corresponding
// timespec components of from "from - ex"
//
// Use this instead of rscfl_timespec_diff when you want to accept negative
// results (seconds and/or nanoseconds)
//
void rscfl_timespec_diff_comp(struct timespec *from, const struct timespec *ex);

// timespec end will be updated to contain the duration between start and
// end; if end < start, end will be set to zero
void rscfl_timespec_diff(struct timespec *end, const struct timespec *start);

// returns -1 if time1 < time2
//          0 if time1 = time2
//          1 if time1 > time2
int rscfl_timespec_compare(struct timespec *time1, struct timespec *time2);
#ifdef __cplusplus
}
#endif
#endif
