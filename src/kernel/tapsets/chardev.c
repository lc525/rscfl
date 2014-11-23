#include "rscfl/kernel/chardev.h"

#include <linux/cdev.h>
#include <linux/rwlock_types.h>

#include "rscfl/config.h"
#include "rscfl/costs.h"
#include "rscfl/res_common.h"
#include "rscfl/kernel/cpu.h"
#include "rscfl/kernel/stap_shim.h"

static struct cdev rscfl_data_cdev;
static struct cdev rscfl_ctrl_cdev;

static struct class *data_class, *ctrl_class;

static int data_mmap(struct file *, struct vm_area_struct *);
static int ctrl_mmap(struct file *, struct vm_area_struct *);

static struct file_operations data_fops = {.mmap = data_mmap, };
static struct file_operations ctrl_fops = {.mmap = ctrl_mmap, };

static int drv_dev_uevent_r(struct device *dev, struct kobj_uevent_env *env) {
  add_uevent_var(env, "DEVMODE=%#o", 0444);
  return 0;
}
static int drv_dev_uevent_rw(struct device *dev, struct kobj_uevent_env *env) {
  add_uevent_var(env, "DEVMODE=%#o", 0666);
  return 0;
}


static int drv_init(int major, int minor, char *drv_name, _Bool dev_rw,
                    struct file_operations *fops, struct cdev *cdev,
                    struct class **class)
{
  int rc;
  int dev_no = MKDEV(major, minor);
  cdev_init(cdev, fops);
  cdev->owner = THIS_MODULE;
  cdev->ops = fops;
  rc = cdev_add(cdev, dev_no, 1);
  if (rc) {
    return -1;
  }

  *class = class_create(THIS_MODULE, drv_name);
  if (dev_rw) {
    (*class)->dev_uevent = drv_dev_uevent_rw;
  } else {
    (*class)->dev_uevent = drv_dev_uevent_r;
  }
  device_create(*class, NULL, dev_no, NULL, drv_name);
  if (rc < 0) {
    return rc;
  }

  return 0;
}

int _rscfl_dev_init(void)
{
  int rc;
  debugk("Init data driver\n");
  rc = drv_init(RSCFL_DATA_MAJOR, RSCFL_DATA_MINOR, RSCFL_DATA_DRIVER, 0,
                &data_fops, &rscfl_data_cdev, &data_class);
  if (rc) {
    return rc;
  }
  debugk("Init ctrl driver\n");
  rc = drv_init(RSCFL_CTRL_MAJOR, RSCFL_CTRL_MINOR, RSCFL_CTRL_DRIVER, 1,
                &ctrl_fops, &rscfl_ctrl_cdev, &ctrl_class);
  if (rc) {
    printk(KERN_ERR "Cannot initialise ctrl driver\n");
    // TODO(oc243): uninitialise the data driver.
  }
  return rc;
}

int _rscfl_dev_cleanup(void)
{
  device_destroy(data_class, MKDEV(RSCFL_DATA_MAJOR, RSCFL_DATA_MINOR));
  device_destroy(ctrl_class, MKDEV(RSCFL_CTRL_MAJOR, RSCFL_CTRL_MINOR));
  class_destroy(data_class);
  class_destroy(ctrl_class);
  return 0;
}

/*
 * Alloc and mmap some memory. Return the address of the memory through
 * mapped_mem.
 */

static int mmap_common(struct file *filp, struct vm_area_struct *vma,
                       char **mapped_mem)
{
  unsigned long page;
  unsigned long pos;
  unsigned long size = (unsigned long)vma->vm_end - vma->vm_start;
  unsigned long start = (unsigned long)vma->vm_start;
  char *shared_buf;

  if (size > MMAP_BUF_SIZE) return -EINVAL;

  shared_buf = kzalloc(MMAP_BUF_SIZE, GFP_KERNEL);
  if (!shared_buf) {
    return -ENOMEM;
  }
  *mapped_mem = shared_buf;

  // do the actual mmap-ing of shared_buf (kernel memory) into the address space
  // of the calling process (user space)
  pos = (unsigned long)shared_buf;

  while (size > 0) {
    page = virt_to_phys((void *)pos);
    if (remap_pfn_range(vma, start, page >> PAGE_SHIFT, MMAP_BUF_SIZE,
                        PAGE_SHARED)) {
      kfree(shared_buf);
      return -EAGAIN;
    }
    start += MMAP_BUF_SIZE;
    pos += MMAP_BUF_SIZE;
    size -= MMAP_BUF_SIZE;
  }
  return 0;
}


/*
 * Perform memory mapping for the data driver. That is to say the driver
 * that stores struct accountings and struct subsys_accountings.
 */
static int data_mmap(struct file *filp, struct vm_area_struct *vma)
{
  pid_acct *pid_acct_node;
  kprobe_priv *probe_data;
  char *shared_data_buf;
  int rc;

  // new pid wants resource accounting data, so add (pid, shared_data_buf) into
  // per-cpu hash table.
  //
  // TODO(lc525, review discussion): decide on whether to add (pid, shared_buf)
  // into the hash tables of every CPU or just in the hash table of the CPU
  // currently running the app. If we get switched a lot between CPUs it might
  // pay off to pre-add entries at the expense of some memory.
  //
  pid_acct_node = (pid_acct *)kzalloc(sizeof(pid_acct), GFP_KERNEL);
  if (!pid_acct_node) {
    return -ENOMEM;
  }
  probe_data = (kprobe_priv *)kzalloc(sizeof(kprobe_priv), GFP_KERNEL);
  if (!probe_data) {
    kfree(pid_acct_node);
    return -ENOMEM;
  }

  if ((rc = mmap_common(filp, vma, &shared_data_buf))) {
    kfree(probe_data);
    kfree(pid_acct_node);
    return rc;
  }
  pid_acct_node->pid = current->pid;
  pid_acct_node->shared_buf = (rscfl_shared_mem_layout_t *)shared_data_buf;
  pid_acct_node->probe_data = probe_data;
  preempt_disable();
  hash_add(CPU_TBL(pid_acct_tbl), &pid_acct_node->link, pid_acct_node->pid);
  CPU_VAR(current_acct) = pid_acct_node;
  preempt_enable();
  return 0;
}


/*
 * Perform the mmap for the memory shared between resourceful kernel and user
 * API. The location of the mapped page is stored in current_pid_acct->ctrl.
 */
static int ctrl_mmap(struct file *filp, struct vm_area_struct *vma)
{
  int rc;
  char *shared_ctrl_buf;
  pid_acct *current_pid_acct;

  if ((rc = mmap_common(filp, vma, &shared_ctrl_buf))) {
    return rc;
  }

  preempt_disable();

  current_pid_acct = CPU_VAR(current_acct);
  // We need to store the address of the control page for the pid, so we
  // can see when an interest is raised.
  current_pid_acct->ctrl = (syscall_interest_t *)shared_ctrl_buf;
  preempt_enable();

  return 0;
}
