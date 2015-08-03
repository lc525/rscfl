#ifndef _RSCFL_KAMPROBES_H_
#define _RSCFL_KAMPROBES_H_

#include <linux/types.h>

int kamprobes_init(int max_probes);

void kamprobes_free(void);

void kamprobes_unregister_all(void);

int kamprobes_register(u8 **orig_addr, char sys_type, void (*pre_handler)(void),
                       void (*post_handler)(void));
#endif

