#include "rscfl/kernel/chardev.h"

#include <linux/cdev.h>
#include <linux/rwlock_types.h>

#include "rscfl/config.h"
#include "rscfl/costs.h"
#include "rscfl/res_common.h"
#include "rscfl/kernel/cpu.h"
#include "rscfl/kernel/stap_shim.h"

static struct cdev rscfl_cdev;
static struct class *rscfl_class;

static struct file_operations fops = {
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

/* A process/thread needing resource accounting will first mmap
 * a character device within it's address space.
 *
 * rscfl_mmap allocates a new accounting buffer (an array of struct accounting),
 * creates kernel side data structures to map the pid to its accounting
 * buffers and then does the actual mmap-ing.
 */
static int rscfl_mmap(struct file *filp, struct vm_area_struct *vma)
{
  unsigned long page;
  unsigned long pos;
  unsigned long size = (unsigned long)vma->vm_end - vma->vm_start;
  unsigned long start = (unsigned long)vma->vm_start;
  char *shared_buf;
  pid_acct *pid_acct_node;
  kprobe_priv *probe_data;

  if (size > MMAP_BUF_SIZE) return -EINVAL;

  shared_buf = kzalloc(MMAP_BUF_SIZE, GFP_KERNEL);
  if (!shared_buf) {
    return -ENOMEM;
  }

  // new pid wants resource accounting data, so add (pid, shared_buf) into
  // per-cpu hash table.
  //
  // TODO(lc525, review discussion): decide on whether to add (pid, shared_buf)
  // into the hash tables of every CPU or just in the hash table of the CPU
  // currently running the app. If we get switched a lot between CPUs it might
  // pay off to pre-add entries at the expense of some memory.
  //
  pid_acct_node = (pid_acct *)kmalloc(sizeof(pid_acct), GFP_KERNEL);
  if(!pid_acct_node) {
    kfree(shared_buf);
    return -ENOMEM;
  }
  probe_data = (kprobe_priv *)kzalloc(sizeof(kprobe_priv), GFP_KERNEL);
  if(!probe_data) {
    kfree(pid_acct_node);
    kfree(shared_buf);
    return -ENOMEM;
  }
  pid_acct_node->curr_subsys = -1;
  pid_acct_node->pid = current->pid;
  pid_acct_node->shared_buf = (rscfl_shared_mem_layout_t *)shared_buf;
  pid_acct_node->probe_data = probe_data;
  preempt_disable();
  hash_add(CPU_TBL(pid_acct_tbl), &pid_acct_node->link, pid_acct_node->pid);
  CPU_VAR(current_acct) = pid_acct_node;
  preempt_enable();

  // do the actual mmap-ing of shared_buf (kernel memory) into the address space
  // of the calling process (user space)
  pos = (unsigned long)shared_buf;

  while (size > 0) {
    page = virt_to_phys((void *)pos);
    if (remap_pfn_range(vma, start, page >> PAGE_SHIFT, MMAP_BUF_SIZE,
                        PAGE_SHARED)) {
      kfree(probe_data);
      kfree(pid_acct_node);
      kfree(shared_buf);
      return -EAGAIN;
    }
    start += MMAP_BUF_SIZE;
    pos += MMAP_BUF_SIZE;
    size -= MMAP_BUF_SIZE;
  }
  return 0;
}
