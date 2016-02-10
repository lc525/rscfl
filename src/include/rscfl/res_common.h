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
#define STRUCT_ACCT_NUM 15
#define ACCT_SUBSYS_RATIO 7   // assume one syscall touches ~ ACCT_SUBSYS_RATIO subsystems
#define MAX_TOKENS 32
#define NUM_READY_TOKENS 12   // Number of tokens that the kernel can prepare
                              // in advance.
// special tokens
#define DEFAULT_TOKEN -15
#define NULL_TOKEN -14
#define IS_USER_TOKEN(token_id) (token_id >= 0)

#define RSCFL_SYSCALL_ID_OFFSET 10

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

/* Configuration and IOCTLS
 */
#define RSCFL_PID_SELF -1

#define RSCFL_SHDW_CMD _IOWR('R', 0x2F, struct rscfl_ioctl)
#define RSCFL_CONFIG_CMD _IOW('R', 0x30, struct rscfl_config)
#define RSCFL_SHUTDOWN_CMD _IO('R', 0x31)
#define RSCFL_NEW_TOKENS_CMD _IO('R', 0x32)
#define RSCFL_DEBUG_CMD _IOW('R', 0x34, struct rscfl_debug)

/*
 * Shadow kernels.
 */
typedef enum {NOP, SPAWN_ONLY, SPAWN_SWAP_ON_SCHED, SWAP} shdw_op;
typedef int shdw_hdl;

struct rscfl_acct_layout_t
{
  struct accounting acct[STRUCT_ACCT_NUM];
  struct subsys_accounting subsyses[ACCT_SUBSYS_NUM];
  int subsys_exits;
};
typedef struct rscfl_acct_layout_t rscfl_acct_layout_t;

/*
 * Expressing interest in resources consumed by syscalls
 */

typedef enum {
  ID_RSCFL_IGNORE = 1,
  ID_NO_ACCT  = 2,
  ID_RSCFL_RESET = 3,
} syscall_special_id;

/*
 *
 */
typedef enum {
  ACCT_DEFAULT      = EBIT(2),     // by default, you get ACCT_NEXT_FL behavior
  ACCT_START        = EBIT(0),     // start accounting
  ACCT_STOP         = EBIT(1),     // stop accounting (global, across tokens)
  ACCT_NEXT_FL      = EBIT(2),     // one-shot; account for the next syscall
  TK_STOP_FL        = EBIT(3),     // stop accounting for given token
  TK_RESET_FL       = EBIT(4),     // Reset the accounting that corresponds
                                   // to the currently active token.
                                   // Also clears the corresponding subsystem
                                   // data.

  ACCT_KNOP          = EBIT(6),   // For benchmarking calibration: run
                                  // acct_next but don't actually express
                                  // interest (no kernel-side effects)
  __ACCT_ERR         = EBIT(7),
  __ACCT_FLAG_IS_PERSISTENT        = (EBIT(0) | EBIT(1) | EBIT(7)),

} interest_flags;



struct syscall_interest_t
{
  unsigned long syscall_id;
  volatile short token_id;
  volatile int tail_ix;
  interest_flags flags;
  shdw_hdl use_shdw;
  int shdw_pages;
  _Bool first_measurement;
  //_Bool token_swapped;
};
typedef struct syscall_interest_t syscall_interest_t;

struct rscfl_ctrl_layout_t
{
  unsigned int version;
  volatile syscall_interest_t interest;
  rscfl_config config;

  int avail_token_ids[NUM_READY_TOKENS];
  int num_avail_token_ids;
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

struct rscfl_debug
{
  char msg[5];
  int new_token_id;
};
typedef struct rscfl_debug rscfl_debug;

#ifdef __cplusplus
extern "C" {
#endif
void rscfl_init_default_config(rscfl_config* default_cfg);

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
