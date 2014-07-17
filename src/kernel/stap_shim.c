#include "rscfl/kernel/stap_shim.h"
#include "rscfl/res_common.h"
#include "rscfl/costs.h"

/*
#include <asm/byteorder.h>
#include <linux/ip.h>
#include <linux/sched.h>
*/

#include <linux/rwlock_types.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/mm.h>
#include <linux/cdev.h>
#include <asm/atomic.h>

//#define BUF_SIZE 40960  // need to think about this
#define BUF_SIZE sizeof(struct accounting) * ACCT_REG_SZ
#define RSCFL_MAJOR 90
#define RSCFL_MINOR 0

struct syscall_acct_list_t {
  unsigned long syscall_id;
  pid_t pid;
  int syscall_nr;
  struct syscall_acct_list_t *next;
};

struct rscfl_pid_pages_t {
  char *buf;
  struct rscfl_pid_pages_t *next;
  pid_t pid;
};

typedef struct syscall_acct_list_t syscall_acct_list_t;

static syscall_acct_list_t *syscall_acct_list;
static long syscall_id_c;
static struct accounting *acct = NULL;
static struct rscfl_pid_pages_t *rscfl_pid_pages;

static rwlock_t lock = __RW_LOCK_UNLOCKED(lock);
static rwlock_t pid_pages_lock = __RW_LOCK_UNLOCKED(pid_pages_lock);

static int rscfl_mmap(struct file *, struct vm_area_struct *);

static struct cdev rscfl_cdev;
static struct class *rscfl_class;

static struct file_operations fops =
{
  .mmap = rscfl_mmap,
};


static int rscfl_mmap(struct file *filp, struct vm_area_struct *vma)
{
  unsigned long page;
  unsigned long pos;
  unsigned long size = (unsigned long)vma->vm_end-vma->vm_start;
  unsigned long start = (unsigned long)vma->vm_start;
  struct rscfl_pid_pages_t *new_hd;
  char *buf;

  if (size > BUF_SIZE)
    return -EINVAL;

  buf = kzalloc(BUF_SIZE, GFP_KERNEL);
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

int _rscfl_shim_init(void)
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

int _rscfl_shim_cleanup(void)
{
  device_destroy(rscfl_class, MKDEV(RSCFL_MAJOR, RSCFL_MINOR));
  class_destroy(rscfl_class);
  return 0;
}


int _fill_struct(long cycles, long wall_clock_time, struct accounting *acct, long fill_type)
{
  debugk("_fill_struct %p %ld %ld %ld\n", (void*)acct, cycles, wall_clock_time, fill_type);
  switch(fill_type) {
    case FILL_MM:
      acct->mm.cycles += cycles;
      break;
    case FILL_FS:
      acct->fs.cycles += cycles;
      break;
    case FILL_NET:
      acct->net.cycles += cycles;
      break;
  }
  acct->cpu.cycles += cycles;
  acct->cpu.wall_clock_time += wall_clock_time;
  return 0;
}

/**
 * if syscall_nr==-1 then we account for the next syscall, independent of which
 * syscall is executed.
 **/
struct accounting * _should_acct(pid_t pid, int syscall_nr)
{
  syscall_acct_list_t *e;
  struct accounting *ret;
  struct rscfl_pid_pages_t *pid_page = rscfl_pid_pages;

  read_lock(&lock);
  //debugk("_should_acct(?) pid: %d\n", pid);
  e = syscall_acct_list;
  while (e) {
    if ((e->pid == pid) &&
       ((syscall_nr == -1) || (e->syscall_nr == syscall_nr))) {
      while (pid_page) {
        if (pid_page->pid == current->pid) {
          if(acct != NULL) {
            debugk("_should_acct(yes, nr %d) %d, into %p\n", e->syscall_nr, pid, (void*)acct);
            return acct;
          }
          read_unlock(&lock);
          ret = (struct accounting *) pid_page->buf;
          BUG_ON(!ret);
          while (ret->in_use == 1) {
          //while (test_and_set_bit(RSCFL_ACCT_USE_BIT, &ret->in_use)) {
            debugk("_should_acct: %p\n", (void *)ret);
            ret++;
            if ((void *)ret > (void *)pid_page->buf + BUF_SIZE) {
              ret = (struct accounting *) pid_page->buf;
              break;
            }
          }
          ret->syscall_id.pid = pid;
          ret->syscall_id.id = e->syscall_nr;
          ret->in_use = 1;
          debugk("_should_acct(yes, nr %d) %d, into %p\n", e->syscall_nr, pid, (void*)ret);
          return ret;
        } else {
          //pid_page++;
          //if (pid_page - rscfl_pid_pages >= BUF_SIZE / sizeof(pid_page)) {
          if (pid_page->next == NULL) {
            read_unlock(&lock);
            printk(KERN_ERR "rscfl: pid %d cannot find mapped page\n",
                         current->pid);
                  return NULL;
          }
          pid_page = pid_page->next;
        }
      }
    }
    e = e->next;
  }
  read_unlock(&lock);
  return NULL;
}

int acct_next(pid_t pid, int syscall_nr)
{
  syscall_acct_list_t *to_acct = (syscall_acct_list_t *)
    kzalloc(sizeof(syscall_acct_list_t), GFP_KERNEL);
  debugk("acct_next %d\n", pid);
  if (!to_acct) {
    return -1;
  }
  to_acct->syscall_id = syscall_id_c++;
  to_acct->pid = pid;
  to_acct->syscall_nr = syscall_nr;
  to_acct->next = syscall_acct_list;
  write_lock(&lock);
  syscall_acct_list = to_acct;
  write_unlock(&lock);
  return 0;
}

/**
 * if syscall_nr==-1 then all resource consumption requests for the given pid
 * are cleared.
 *
 * if pid==-1 then syscall_nr will be cleared regardless of its associated pid
 *
 * if pid==-1 && syscall_nr==-1 then the resource consumption list is cleared
 **/
int _clear_acct_next(pid_t pid, int syscall_nr)
{
  syscall_acct_list_t *entry;
  syscall_acct_list_t *prev = NULL;
  syscall_acct_list_t *next;
  int rc = -1;
  debugk("clear_acct_next %d\n", pid);

  read_lock(&lock);
  entry = syscall_acct_list;

  while (entry) {
    debugk("clear_acct_next: in while\n");
    if (((syscall_nr == -1) || (syscall_nr == entry->syscall_nr)) &&
        ((pid == -1) || (pid = entry->pid))) {
      if (prev) {
        prev->next = entry->next;
      } else {
        syscall_acct_list = entry->next;
      }
      next = entry->next;
      kfree(entry);
      if (syscall_nr > 0) {
        read_unlock(&lock);
        return 0;
      }
      rc = 0;

      entry = next;

    } else {
      prev = entry;
      entry = entry->next;
    }
  }
  read_unlock(&lock);
  return rc;
}
