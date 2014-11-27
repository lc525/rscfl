#ifndef _RES_API_H_
#define _RES_API_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "rscfl/costs.h"
#include "rscfl/res_common.h"
#include "rscfl/subsys_list.h"

#define MAX_PAYLOAD 1024 /* maximum payload size*/
#define SUBSYS_AS_STR_ARRAY(a, b, c) [a] = c,

/*
 * RSCFL API data structures
 */

extern const char *rscfl_subsys_name[NUM_SUBSYSTEMS];

struct rscfl_handle_t {
  char *buf;
  rscfl_syscall_id_t lst_syscall;

  syscall_interest_t *interests;
};
typedef struct rscfl_handle_t *rscfl_handle;

struct subsys_idx_set {
  short idx[NUM_SUBSYSTEMS];
  struct subsys_accounting *set;
  short set_size;
  short max_set_size;
};
typedef struct subsys_idx_set subsys_idx_set;


/*
 * Basic API
 */

rscfl_handle rscfl_init();

rscfl_handle rscfl_get_handle();

int rscfl_acct_next(rscfl_handle);

int rscfl_read_acct(rscfl_handle handle, struct accounting *acct);


/* -- low level API functions --
 *
 * only use those functions explicitly if you know what you're doing
 *
 * for example, using rscfl_get_subsys_by_id does not free the kernel-side
 * resources allocated for storing per-subsystem measurement data. You would
 * need to explicitly set subsys->in_use to 0 or call rscfl_subsys_free
 * afterwards.
 *
 * failing to use the proper calling protocol of those functions might lead to
 * kernel panics.
 *
 */
void rscfl_subsys_merge(struct subsys_accounting *existing_subsys,
                        struct subsys_accounting *new_subsys);
struct subsys_accounting* rscfl_get_subsys_by_id(rscfl_handle rhdl,
                                                 struct accounting *acct,
                                                 rscfl_subsys subsys_id);
void rscfl_subsys_free(rscfl_handle rhdl, struct accounting *acct);


/* -- high level API functions --
 */
subsys_idx_set* rscfl_get_subsys(rscfl_handle rhdl, struct accounting *acct);
subsys_idx_set* rscfl_get_new_aggregator(unsigned short no_subsystems);
int rscfl_merge_acct_into(rscfl_handle rhdl, struct accounting *acct_from,
                          subsys_idx_set *aggregator_into);


/*
 * Advanced API
 */

#define DEFINE_SELECT_FCT_PTR(pname, rtype) \
  typedef rtype (*subsys_select_##pname)(struct subsys_accounting*)

#define DEFINE_COMBINE_FCT_PTR(pname, rtype) \
  typedef void (*subsys_combine_##pname)(rtype*, rtype)

#define SELECT_FCT_PTR(pname) subsys_select_##pname
#define COMBINE_FCT_PTR(pname) subsys_combine_##pname

#define DEFINE_REDUCE_FUNCTION(pname, rtype)                                   \
rtype rscfl_subsys_reduce_##pname(rscfl_handle rhdl, struct accounting* acct,  \
                                  int free_subsys,                             \
                                  rtype ret_on_err,                            \
                                  SELECT_FCT_PTR(pname) select,                \
                                  COMBINE_FCT_PTR(pname) combine)              \
{                                                                              \
  int i;                                                                       \
  rtype accum;                                                                 \
  if(acct == NULL) return ret_on_err;                                          \
                                                                               \
  for(i = 0; i < NUM_SUBSYSTEMS; ++i) {                                        \
    struct subsys_accounting *subsys =                                         \
      rscfl_get_subsys_by_id(rhdl, acct, (rscfl_subsys)i);                     \
    rtype current;                                                             \
    if(subsys != NULL) {                                                       \
      current = select(subsys);                                                \
      if(free_subsys) subsys->in_use = 0;                                      \
      combine(&accum, current);                                                \
    }                                                                          \
  }                                                                            \
                                                                               \
  return accum;                                                                \
}

#define REDUCE_SUBSYS(pname, rhdl, acct, free_subsys, select, combine)         \
  rscfl_subsys_reduce_##pname(rhdl, acct, free_subsys, select, combine)


DEFINE_SELECT_FCT_PTR(lluint, ru64);
DEFINE_COMBINE_FCT_PTR(lluint, ru64);

DEFINE_REDUCE_FUNCTION(lluint, ru64)

#ifdef __cplusplus
}
#endif
#endif
