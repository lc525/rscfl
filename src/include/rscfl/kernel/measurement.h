#ifndef _RSCFL_MEASUREMENT_H_
#define _RSCFL_MEASUREMENT_H_

#include <linux/hashtable.h>

#include "rscfl/costs.h"

// Build a hash table with 256 bins for tokens.
#define TOKENS_HASH_BUCKETS 8


extern ru64 no_evtchn_events;

struct rscfl_kernel_token
{
  int id;
  int val;
  struct hlist_node link;
};

extern DECLARE_HASHTABLE(tokens, TOKENS_HASH_BUCKETS);

int rscfl_counters_init(void);

void rscfl_counters_stop(void);

int xen_buffer_hd(void);

int rscfl_counters_update_subsys_vals(struct subsys_accounting *add_subsys,
                                      struct subsys_accounting *minus_subsys);

#endif /* _MEASUREMENT_H_ */

