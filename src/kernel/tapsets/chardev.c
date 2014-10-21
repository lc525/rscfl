#include "rscfl/kernel/chardev.h"

#include <linux/cdev.h>
#include <linux/rwlock_types.h>

#include "rscfl/config.h"
#include "rscfl/res_common.h"
#include "rscfl/kernel/stap_shim.h"

static struct cdev rscfl_cdev;
static struct class *rscfl_class;
static rwlock_t pid_pages_lock = __RW_LOCK_UNLOCKED(pid_pages_lock);

static struct file_operations fops =
{
  .mmap = rscfl_mmap,
};

int _rscfl_dev_init(void)
{
  int rc;
  int dev_no = MKDEV(RSCFL_MAJOR, RSCFL_MINOR);
  cdev_init(&rscfl_cdev, &fops);
  rscfl_cdev.owner = THIS_MODULE;
  rscfl_cdev.ops = &fops;
  rc = cdev_add(&rscfl_cdev, dev_no, 1);
  if (rc) {
    return -1;
  }

  rscfl_class = class_create(THIS_MODULE, RSCFL_DRIVER);
  device_create(rscfl_class, NULL, dev_no, NULL, RSCFL_DRIVER);
  if (rc < 0) {
    return rc;
  }

  return 0;
}

int _rscfl_dev_cleanup(void)
{
  device_destroy(rscfl_class, MKDEV(RSCFL_MAJOR, RSCFL_MINOR));
  class_destroy(rscfl_class);
  return 0;
}

static int rscfl_mmap(struct file *filp, struct vm_area_struct *vma)
{
  unsigned long page;
  unsigned long pos;
  unsigned long size = (unsigned long)vma->vm_end - vma->vm_start;
  unsigned long start = (unsigned long)vma->vm_start;
  rscfl_pid_pages_t *new_hd;
  char *buf;

  if (size > MMAP_BUF_SIZE)
    return -EINVAL;

  buf = kzalloc(MMAP_BUF_SIZE, GFP_KERNEL);
  if (!buf) {
    return -1;
  }
  write_lock(&pid_pages_lock);
  new_hd = (struct rscfl_pid_pages_t *) kmalloc(sizeof(struct rscfl_pid_pages_t),
            GFP_KERNEL);
  if (!new_hd) {
    kfree(buf);
    return -1;
  }
  new_hd->buf = buf;
  new_hd->pid = current->pid;
  new_hd->next = rscfl_pid_pages;
  rscfl_pid_pages = new_hd;
  write_unlock(&pid_pages_lock);

  pos = (unsigned long)buf;

  while (size) {
    page = virt_to_phys((void *)pos);
    if (remap_pfn_range(vma, start, page >> PAGE_SHIFT, PAGE_SIZE, PAGE_SHARED)) {
      rscfl_pid_pages = rscfl_pid_pages->next;
      kfree(new_hd);
      kfree(buf);
      return -EAGAIN;
    }
    start+=PAGE_SIZE;
    pos+=PAGE_SIZE;
    size-=PAGE_SIZE;
  }
  return 0;
}

