#include "rscfl/kernel/stap_shim.h"

#include <asm/atomic.h>
#include <linux/rwlock_types.h>
#include <linux/slab.h>
#include <linux/spinlock.h>

#include "rscfl/config.h"
#include "rscfl/costs.h"
#include "rscfl/res_common.h"

static long syscall_id_c;
static syscall_acct_list_t *syscall_acct_list;
static struct rscfl_pid_pages_t *rscfl_pid_pages;

static rwlock_t lock = __RW_LOCK_UNLOCKED(lock);


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
 * if syscall_nr==-1 then we account for the next syscall, independent of which
 * syscall is executed.
 **/
int _should_acct(pid_t pid, int syscall_nr, struct accounting **acct,
                 int probe_nest, const char *name)
{
  syscall_acct_list_t *e;
  struct accounting *ret;
  struct rscfl_pid_pages_t *pid_page = rscfl_pid_pages;

  read_lock(&lock);
  // debugk("_should_acct(?) pid: %d\n", pid);
  e = syscall_acct_list;
  while (e) {
    if ((e->pid == pid) &&
        ((syscall_nr == -1) || (e->syscall_nr == syscall_nr))) {
      while (pid_page) {
        if (pid_page->pid == pid) {
          debugk("\t acct: %p\n", (void *)(*acct));
          if (*acct != NULL) {
            read_unlock(&lock);
            return 1;
          }
          read_unlock(&lock);

          ret = (struct accounting *)pid_page->buf;
          BUG_ON(!ret);
          while (ret->in_use == 1) {
            // while (test_and_set_bit(RSCFL_ACCT_USE_BIT, &ret->in_use)) {
            debugk("in use: %p for (pid, id):(%d, %ld)\n", (void *)ret,
                   ret->syscall_id.pid, ret->syscall_id.id);
            ret++;
            if ((void *)ret > (void *)pid_page->buf + MMAP_BUF_SIZE) {
              ret = (struct accounting *)pid_page->buf;
              debugk("_should_acct: wraparound!<<<<<<<\n");
              break;
            }
          }
          ret->in_use = 1;
          ret->syscall_id.pid = pid;
          ret->syscall_id.id = e->syscall_nr;
          debugk("_should_acct %s: (yes, nr %d) %d, into %p\n", name,
                 e->syscall_nr, pid, (void *)ret);
          *acct = ret;
          return 1;
        } else {
          // pid_page++;
          // if (pid_page - rscfl_pid_pages >= MMAP_BUF_SIZE / sizeof(pid_page)) {
          if (pid_page->next == NULL) {
            read_unlock(&lock);
            printk(KERN_ERR "rscfl: pid %d cannot find mapped page\n", pid);
            *acct = NULL;
            return 0;
          }
          pid_page = pid_page->next;
        }
      }
    }
    e = e->next;
  }
  read_unlock(&lock);
  return 0;
}

int _fill_struct(long cycles, long wall_clock_time, struct accounting *acct,
                 long fill_type)
{
  debugk("_fill_struct acct:%p cy:%ld wc:%ld type:%ld\n", (void *)acct, cycles,
         wall_clock_time, fill_type);
  switch (fill_type) {
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
