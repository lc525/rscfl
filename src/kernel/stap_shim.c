#include "res_kernel/stap_shim.h"
#include "costs.h"
#include <linux/rwlock_types.h>
#include <linux/spinlock.h>

struct syscall_acct_list_t {
  struct accounting *acct;
  unsigned long syscall_id;
  pid_t pid;
  int syscall_nr;
  struct syscall_acct_list_t *next;
};

struct free_accounting_pool {
  struct accounting *item;
  struct free_accounting_pool *prev;
  struct free_accounting_pool *next;
};

typedef struct syscall_acct_list_t syscall_acct_list_t;

static syscall_acct_list_t *syscall_acct_list;
static struct rchan *chan;
static struct free_accounting_pool *free_acct_pool_hd, *free_acct_pool_lst;
static long syscall_id_c;

static rwlock_t lock = __RW_LOCK_UNLOCKED(lock);
static spinlock_t free_acct_lock = __SPIN_LOCK_UNLOCKED(free_acct_lock);

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

static inline void return_to_pool (struct accounting *acct)
{
  spin_lock(&free_acct_lock);
  BUG_ON(unlikely(!(free_acct_pool_lst || free_acct_pool_hd)));
  BUG_ON(unlikely(free_acct_pool_lst->next));
  BUG_ON(unlikely(free_acct_pool_lst->item));
  free_acct_pool_lst->next = free_acct_pool_hd;
  free_acct_pool_hd = free_acct_pool_lst;
  if (free_acct_pool_lst->prev) {
    free_acct_pool_lst = free_acct_pool_lst->prev;
     free_acct_pool_hd->next->prev = free_acct_pool_hd;
  }
  free_acct_pool_hd->item = acct;
  free_acct_pool_hd->prev = NULL;
  free_acct_pool_lst->next = NULL;
  BUG_ON(unlikely(!(free_acct_pool_lst || free_acct_pool_hd)));
  spin_unlock(&free_acct_lock);
}


/**
 * Get memory to store the accounting in. Prefer reusing memory rather than
 * kmalloc-ing more.
 **/
static inline struct accounting * fetch_from_pool(void)
{
  struct accounting *acct;
  spin_lock(&free_acct_lock);
  if (free_acct_pool_hd && free_acct_pool_hd->item) {
    acct = free_acct_pool_hd->item;
    free_acct_pool_hd->item = NULL;
    free_acct_pool_lst->next = free_acct_pool_hd;
    free_acct_pool_hd = free_acct_pool_hd->next;
    free_acct_pool_lst->next = NULL;
    free_acct_pool_hd = NULL;
    spin_unlock(&free_acct_lock);
  }
  else {
    if (free_acct_pool_lst) {
      // No free struct accountings, create memory for a new one
      struct free_accounting_pool *to_ins =
        kzalloc(sizeof(struct free_accounting_pool), GFP_KERNEL);
      if (!to_ins) {
        return NULL;
      }
      to_ins->prev = free_acct_pool_lst;
      free_acct_pool_lst->next = to_ins;
    }
    else {
      // Need to initialise pool
      free_acct_pool_hd = free_acct_pool_lst = (struct free_accounting_pool *)
        kzalloc(sizeof(struct free_accounting_pool), GFP_KERNEL);
      if (!free_acct_pool_hd) {
        return NULL;
      }
    }
    spin_unlock(&free_acct_lock);
    /**
     * No need to lock on elements of the pool.
     */
    acct = (struct accounting *) kzalloc(sizeof(struct accounting),
                                                  GFP_KERNEL);
    if (!acct) {
      //TODO: We currently leak here :-(
      return NULL;
    }
  }
  return acct;
}

int _create_shared_mem(void)
{
  debugk("_create_shared_mem\n");

  if ( !(chan = relay_open(PROJECT_NAME, NULL, SUBBUF_SIZE,
         N_SUBBUFS, &relay_callbacks, NULL)) ) {
    printk(KERN_ERR "rscfl: cannot open relay channel\n");
    goto error;
  }
  return 0;

error:
  return -1;
}

int _rscfl_shim_init(void)
{
  return _create_shared_mem();
}


int _clean_debugfs(void)
{
  relay_close(chan);
  return 0;
}

int _fill_struct(long cycles, struct accounting *acct)
{
  debugk("_fill_struct\n");
  acct->cpu.cycles = cycles;
  return 0;
}

/**
 * if finalised then all synchronous effects associated with acct have finished.
 * We therefore assume that there will be no further writes to it, and return
 * it to the pool.
 */
int _update_relay(struct accounting *acct, int finalised)
{
  struct free_accounting_pool *head;
  debugk("_update_relay\n");
  relay_write(chan, acct, sizeof(struct accounting));
  if (finalised) {
    // return the struct accounting to the free accounting pool
    return_to_pool(acct);
  }
  return 0;
}


/**
 * if syscall_nr==-1 then we account for the next syscall, independent of which
\ * syscall is executed.
 **/
struct accounting * _should_acct(pid_t pid, int syscall_nr)
{
  syscall_acct_list_t *e;
  struct accounting *ret;

  read_lock(&lock);
  e = syscall_acct_list;
  while (e) {
    if ((e->pid == pid) &&
  ((syscall_nr == -1) || (e->syscall_nr == syscall_nr))) {
      ret = e->acct;
      ret->syscall_id.pid = pid;
      read_unlock(&lock);
      return ret;
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
  if (!to_acct) {
    return -1;
  }
  to_acct->syscall_id = syscall_id_c++;
  to_acct->pid = pid;
  to_acct->syscall_nr = syscall_nr;
  to_acct->next = syscall_acct_list;
  to_acct->acct = fetch_from_pool();

  if (!to_acct->acct) {
    kfree(to_acct);
    return -1;
  }
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
