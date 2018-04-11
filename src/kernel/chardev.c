/**** Notice
 * chardev.c: rscfl source code
 *
 * Copyright 2015-2017 The rscfl owners <lucian.carata@cl.cam.ac.uk>
 *
 * This file is part of the rscfl open-source project: github.com/lc525/rscfl;
 * Its licensing is governed by the LICENSE file at the root of the project.
 **/

#include "rscfl/kernel/chardev.h"

#include <linux/cdev.h>
#include <linux/rwlock_types.h>

#include "rscfl/config.h"
#include "rscfl/costs.h"
#include "rscfl/res_common.h"
#include "rscfl/kernel/acct.h"
#include "rscfl/kernel/cpu.h"
#include "rscfl/kernel/rscfl.h"
#include "rscfl/kernel/shdw.h"

static struct cdev rscfl_data_cdev;
static struct cdev rscfl_ctrl_cdev;

//TODO(lc525): make this safe for concurrency
static struct rscfl_config rscfl_user_config;

struct device *rscfl_ctrl_device;

static struct class *data_class, *ctrl_class;

static int data_mmap(struct file *, struct vm_area_struct *);
static int ctrl_mmap(struct file *, struct vm_area_struct *);
static long rscfl_ioctl(struct file *, unsigned int cmd, unsigned long arg);

static struct file_operations data_fops = {.mmap = data_mmap, };
static struct file_operations ctrl_fops = {
  .mmap = ctrl_mmap,
  .unlocked_ioctl = rscfl_ioctl,
};

struct rscfl_vma_data {
  pid_acct *pid_acct_node;
  char *mmap_shared_buf;
  int ref_count;
};
typedef struct rscfl_vma_data rscfl_vma_data;

static void rscfl_vma_open(struct vm_area_struct *);
static void rscfl_vma_close(struct vm_area_struct *);

static struct vm_operations_struct rscfl_mmap_vm_ops = {
  .open = rscfl_vma_open,
  .close = rscfl_vma_close,
};


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
                    struct class **class, struct device **dev)
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
  *dev = device_create(*class, NULL, dev_no, NULL, drv_name);
  if (rc < 0) {
    return rc;
  }

  return 0;
}

int _rscfl_dev_init(void)
{
  int rc;
  struct device *dev;

  // default rscfl configuration
  rscfl_init_default_config(&rscfl_user_config);

  // initialise devices
  debugk(RDBG_INFO, "Init data driver\n");
  rc = drv_init(RSCFL_DATA_MAJOR, RSCFL_DATA_MINOR, RSCFL_DATA_DRIVER, 0,
                &data_fops, &rscfl_data_cdev, &data_class, &dev);
  if (rc) {
    return rc;
  }
  debugk(RDBG_INFO, "Init ctrl driver\n");
  rc = drv_init(RSCFL_CTRL_MAJOR, RSCFL_CTRL_MINOR, RSCFL_CTRL_DRIVER, 1,
                &ctrl_fops, &rscfl_ctrl_cdev, &ctrl_class, &rscfl_ctrl_device);
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

static void rscfl_vma_open(struct vm_area_struct *vma)
{
  rscfl_vma_data *drv_data = (rscfl_vma_data*) vma->vm_private_data;
  drv_data->ref_count++;
}

static void rscfl_vma_close(struct vm_area_struct *vma)
{
  rscfl_vma_data *drv_data = (rscfl_vma_data*) vma->vm_private_data;

  if(drv_data != NULL) {
    drv_data->ref_count--;

    if (drv_data->ref_count == 0) { // need to clean up

      if(drv_data->pid_acct_node != NULL) {
        // stop any other probes from firing
        drv_data->pid_acct_node->ctrl = NULL;
        drv_data->pid_acct_node->shared_buf = NULL;
      }

      kfree(drv_data->mmap_shared_buf);
      vma->vm_private_data = NULL;
      kfree(drv_data);
    }
  }
}

/*
 * Alloc and mmap some memory. Return the address of the memory through
 * mapped_mem.
 */
static int mmap_common(struct file *filp, struct vm_area_struct *vma,
                       char **mapped_mem, size_t req_length)
{
  unsigned long page;
  unsigned long pos;
  unsigned long size = (unsigned long)vma->vm_end - vma->vm_start;
  unsigned long start = (unsigned long)vma->vm_start;
  struct rscfl_vma_data *drv_data;
  char *shared_buf;

  if (size > req_length) return -EINVAL;

  shared_buf = kzalloc(req_length, GFP_KERNEL);
  //shared_buf = dma_zalloc_coherent(dev, req_length, GFP_KERNEL);
  if (!shared_buf) {
    return -ENOMEM;
  }
  *mapped_mem = shared_buf;

  // do the actual mmap-ing of shared_buf (kernel memory) into the address space
  // of the calling process (user space)
  pos = (unsigned long)shared_buf;
  //vma->vm_page_prot = PAGE_SHARED;
  //vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);

  while (size > 0) {
    page = virt_to_phys((void *)pos);
    if (remap_pfn_range(vma, start, page >> PAGE_SHIFT, req_length,
                        vma->vm_page_prot)) {
      kfree(shared_buf);
      printk(KERN_ERR "ERROR remap_pfn_range!\n");
      return -EAGAIN;
    }
    start += req_length;
    pos += req_length;
    size -= req_length;
  }

  drv_data = kzalloc(sizeof(rscfl_vma_data), GFP_KERNEL);
  if (!drv_data) {
    kfree(shared_buf);
    return -ENOMEM;
  }
  drv_data->ref_count = 0;
  drv_data->mmap_shared_buf = shared_buf;
  vma->vm_private_data = (void*) drv_data;
  vma->vm_ops = &rscfl_mmap_vm_ops;
  rscfl_vma_open(vma); // increment ref count

  return 0;
}


/*
 * Perform memory mapping for the data driver. That is to say the driver
 * that stores struct accountings and struct subsys_accountings.
 */
static int data_mmap(struct file *filp, struct vm_area_struct *vma)
{
  pid_acct *pid_acct_node;
  probe_priv *probe_data;
  char *shared_data_buf;
  struct rscfl_vma_data *drv_data;
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
  probe_data = (probe_priv *)kzalloc(sizeof(probe_priv), GFP_KERNEL);
  if (!probe_data) {
    kfree(pid_acct_node);
    return -ENOMEM;
  }

  if ((rc = mmap_common(filp, vma, &shared_data_buf, MMAP_BUF_SIZE))) {
    kfree(probe_data);
    kfree(pid_acct_node);
    return rc;
  }
  pid_acct_node->subsys_ptr = pid_acct_node->subsys_stack;
  *(pid_acct_node->subsys_ptr) = USERSPACE_LOCAL;
  pid_acct_node->subsys_ptr++;

  if(rscfl_user_config.monitored_pid == RSCFL_PID_SELF) {
    pid_acct_node->pid = current->pid;
  }
  else {
    pid_acct_node->pid = rscfl_user_config.monitored_pid;
  }
  pid_acct_node->shared_buf = (rscfl_acct_layout_t *)shared_data_buf;
  pid_acct_node->shared_buf->subsys_exits = 0;
  pid_acct_node->probe_data = probe_data;
  pid_acct_node->next_ctrl_token = 0;
  pid_acct_node->num_tokens = 0;

  drv_data = (rscfl_vma_data*) vma->vm_private_data;
  drv_data->pid_acct_node = pid_acct_node;
  preempt_disable();
  hash_add(CPU_TBL(pid_acct_tbl), &pid_acct_node->link, pid_acct_node->pid);
  debugk(RDBG_FINE, KERN_WARNING "registering PID: %d\n", pid_acct_node->pid);
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
  int rc, i;
  char *shared_ctrl_buf;
  struct rscfl_vma_data *drv_data;
  rscfl_ctrl_layout_t *ctrl_layout;
  pid_acct *current_pid_acct;

  if ((rc = mmap_common(filp, vma, &shared_ctrl_buf, MMAP_CTL_SIZE))) {
    return rc;
  }

  preempt_disable();

  ctrl_layout = (rscfl_ctrl_layout_t *)shared_ctrl_buf;
  ctrl_layout->version = RSCFL_VERSION.data_layout;
  ctrl_layout->config = rscfl_user_config;
  ctrl_layout->interest.token_id = DEFAULT_TOKEN;
  ctrl_layout->interest.first_measurement = 1;

  // We need to store the address of the control page for the pid, so we
  // can see when an interest is raised.
  current_pid_acct = CPU_VAR(current_acct);
  current_pid_acct->ctrl = ctrl_layout;

  // Initialise kernel-side tokens
  for(i=0; i<NUM_READY_TOKENS; i++) {
    rscfl_kernel_token *token =
            kzalloc(GFP_KERNEL, sizeof(struct rscfl_kernel_token));
    if (token == NULL) {
      break;
    }
    token->id = current_pid_acct->num_tokens++;
    token->val = 0;
    token->val2 = 0;
    current_pid_acct->token_ix[i] = token;
  }
  current_pid_acct->default_token = kzalloc(GFP_KERNEL,
                                            sizeof(struct rscfl_kernel_token));
  current_pid_acct->default_token->id = DEFAULT_TOKEN;
  current_pid_acct->active_token = current_pid_acct->default_token;
  /*
   *current_pid_acct->null_token = kzalloc(GFP_KERNEL,
   *                                       sizeof(struct rscfl_kernel_token));
   *current_pid_acct->null_token->id = NULL_TOKEN;
   */

  drv_data = (rscfl_vma_data*) vma->vm_private_data;
  drv_data->pid_acct_node = current_pid_acct;
  preempt_enable();

  return 0;
}

static long rscfl_ioctl(struct file *f, unsigned int cmd, unsigned long arg)
{
  switch (cmd) {
#if SHDW_ENABLED != 0
    case RSCFL_SHDW_CMD: {
      int rc;
      rscfl_ioctl_t rscfl_arg;
      shdw_hdl shdw;
      copy_from_user(&rscfl_arg, (rscfl_ioctl_t *)arg, sizeof(rscfl_ioctl_t));
      shdw = rscfl_arg.swap_to_shdw;
      rc = do_shdw_op(rscfl_arg.shdw_operation, &shdw, rscfl_arg.num_shdw_pages);
      if (rc) {
        return rc;
      }
      rscfl_arg.new_shdw_id = shdw;
      copy_to_user((rscfl_ioctl_t *)arg, &rscfl_arg, sizeof(rscfl_ioctl_t));
      return 0;
      break;
    }
 #endif /* SHDW_ENABLED */
    case RSCFL_CONFIG_CMD: {
      // This is not safe for multiple applications doing rscfl_init()
      // at the same time.
      copy_from_user(&rscfl_user_config, (rscfl_config *)arg,
                     sizeof(rscfl_config));
      return 0;
      break;
    }
    case RSCFL_DEBUG_CMD: {
      rscfl_debug dbg;
      volatile syscall_interest_t *interest;
      pid_acct *current_pid_acct;
      copy_from_user(&dbg, (rscfl_debug *)arg, sizeof(rscfl_debug));

      current_pid_acct = CPU_VAR(current_acct);
      interest = &current_pid_acct->ctrl->interest;
      if((interest->flags & __ACCT_ERR) == 0)
        printk("IOCTL_CMD: %s, usp_token: %d, active_ktok:%d, ist_ktok:%d, syscall_id:%lu\n",
            dbg.msg, dbg.new_token_id,
            current_pid_acct->active_token->id, interest->token_id,
            interest->syscall_id);
      return 0;
      break;
    }
    case RSCFL_NEW_TOKENS_CMD : {
      int i, j, next, n, ngen;
      pid_acct *current_pid_acct;
      current_pid_acct = CPU_VAR(current_acct);
      if(current_pid_acct != NULL) {
        if(current_pid_acct->ctrl->num_avail_token_ids > 0) return 0;

        next = current_pid_acct->next_ctrl_token;
        n = current_pid_acct->num_tokens;
        if(n >= MAX_TOKENS) {
          printk(KERN_ERR "rscfl: max number of tokens exceeded\n");
          return -EINVAL;
        }

        current_pid_acct->ctrl->num_avail_token_ids = n - next;
        if(n - next == 0) {
          // we'll need to generate some new tokens, up to MAX_TOKENS in total
          if(n + NUM_READY_TOKENS < MAX_TOKENS) ngen = NUM_READY_TOKENS;
          else ngen = MAX_TOKENS - n;
          for(i = n; i < n + ngen ; i++) {
            rscfl_kernel_token *token =
              kzalloc(GFP_KERNEL, sizeof(struct rscfl_kernel_token));
            if (token == NULL) {
              break;
            }
            token->id = current_pid_acct->num_tokens++;
            current_pid_acct->token_ix[i] = token;
          }
          n += ngen;
        }

        // move existing kernel tokens into user-space
        for(i = next, j = 0; i < n; i++) {
          // copy to ctrl
          current_pid_acct->ctrl->avail_token_ids[j++] =
            current_pid_acct->token_ix[i]->id;
        }
        current_pid_acct->ctrl->num_avail_token_ids = n - next;
        current_pid_acct->next_ctrl_token = n;
      } else {
        printk(KERN_ERR "rscfl: pid not registered for requesting tokens\n");
        return -EINVAL;
      }

      return 0;
      break;
    }

    case RSCFL_SHUTDOWN_CMD: {
      do_module_shutdown();
      return 0;
      break;
    }
  }
  return 0;
}
