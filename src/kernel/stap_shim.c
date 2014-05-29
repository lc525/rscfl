#include "res_kernel/stap_shim.h"
#include "costs.h"
#include <linux/rwlock_types.h>
#include <linux/spinlock.h>


struct syscall_acct_list_t {
  unsigned long syscall_id;
  pid_t pid;
  int syscall_nr;
  struct syscall_acct_list_t *next;
};

typedef struct syscall_acct_list_t syscall_acct_list_t;

static syscall_acct_list_t *syscall_acct_list;
static struct rchan *chan;
static struct accounting *acct;
static long syscall_id_c;

static rwlock_t lock = __RW_LOCK_UNLOCKED(lock);

static struct dentry *create_buf_file_handler(const char *, struct dentry *,
                umode_t, struct rchan_buf *,
                int *);

static int remove_buf_file_handler(struct dentry *dentry);

static struct rchan_callbacks relay_callbacks =
{
  .create_buf_file = create_buf_file_handler,
  .remove_buf_file = remove_buf_file_handler,
};

static struct dentry *create_buf_file_handler(const char *filename,
                struct dentry *parent,
                umode_t mode,
                struct rchan_buf *buf,
                int *is_global)
{
  return debugfs_create_file(filename, mode, parent, buf,
           &relay_file_operations);
}

static int remove_buf_file_handler(struct dentry *dentry)
{
  debugfs_remove(dentry);
  return 0;
}


int _create_shared_mem(void)
{
  debugk("_create_shared_mem\n");
  if ( !(acct = ((struct accounting*) kmalloc(sizeof(struct accounting),
                GFP_KERNEL))) ) {
    printk(KERN_ERR "rscfl: cannot kmalloc shared mem\n");
    goto error;
  }

  if ( !(chan = relay_open(PROJECT_NAME, NULL, SUBBUF_SIZE,
         N_SUBBUFS, &relay_callbacks, NULL)) ) {
    kfree(acct);
    printk(KERN_ERR "rscfl: cannot open relay channel\n");
    goto error;
  }
  return 0;

error:
  return -1;
}

int _clean_debugfs(void)
{
  relay_close(chan);
  return 0;
}

int _fill_struct(long cycles)
{
  debugk("_fill_struct\n");
  acct->cpu.cycles = cycles;
  return 0;
}

int _update_relay(void)
{
  debugk("_update_relay\n");
  relay_reset(chan);
  relay_write(chan, acct, sizeof(struct accounting));
  return 0;
}


/**
 * if syscall_nr==-1 then we account for the next syscall, independent of which
\ * syscall is executed.
 **/
int _should_acct(pid_t pid, int syscall_nr)
{
  syscall_acct_list_t *e;

  read_lock(&lock);
  e = syscall_acct_list;
  while (e) {
    if ((e->pid == pid) &&
  ((syscall_nr == -1) || (e->syscall_nr == syscall_nr))) {
      read_unlock(&lock);
      return 1;
    }
    e = e->next;
  }
  read_unlock(&lock);
  return 0;
}

int acct_next(pid_t pid, int syscall_nr)
{
  syscall_acct_list_t *to_acct = (syscall_acct_list_t *)
    kmalloc(sizeof(syscall_acct_list_t), GFP_KERNEL);
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

  read_lock(&lock);
  entry = syscall_acct_list;

  while (entry) {
    if (((syscall_nr == -1) || (syscall_nr == entry->syscall_nr)) &&
        ((pid == -1) || (pid = entry->pid)))
    {
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
