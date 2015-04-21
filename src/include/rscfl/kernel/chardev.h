#ifndef _RSCFL_CHARDEV_H_
#define _RSCFL_CHARDEV_H_

#include <linux/mm.h>

extern struct device *rscfl_ctrl_device;

int _rscfl_dev_init(void);
int _rscfl_dev_cleanup(void);
static int rscfl_mmap(struct file *, struct vm_area_struct *);

#endif
