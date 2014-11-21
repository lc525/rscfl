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

const char *rscfl_subsys_name[NUM_SUBSYSTEMS];

struct rscfl_handle_t
{
  char *buf;
  rscfl_syscall_id_t lst_syscall;

  syscall_interest_t *interests;
};

typedef struct rscfl_handle_t *rscfl_handle;

rscfl_handle rscfl_init();

rscfl_handle rscfl_get_handle();

int rscfl_acct_next(rscfl_handle);

int rscfl_read_acct(rscfl_handle handle, struct accounting *acct);

/* slower version of read that merges (aggregates) current resource data into an
 * existing accounting data structure
 * */
int rscfl_merge_acct(rscfl_handle handle, struct accounting *acct);

/*
 * Get a pointer to the memory that contains the accounting information for
 * subsystem $rscfl_subsys from the struct accounting. We have a helper function
 * for this as we store indices of offsets into the memory, rather than
 * pointers as we need to conserve offsets across the user-kernel memory
 * boundary.
 */
struct subsys_accounting *get_subsys_accounting(rscfl_handle,
                                                struct accounting *,
                                                rscfl_subsys);


#ifdef __cplusplus
}
#endif
#endif
