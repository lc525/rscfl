#ifndef _RSCFL_KAMPROBES_H_
#define _RSCFL_KAMPROBES_H_

#include <linux/types.h>

void kamprobes_unregister_all(void);

int kamprobes_register(u8 **orig_addr, char sys_type, void (*pre_handler)(void),
                       void (*post_handler)(void));
#endif

