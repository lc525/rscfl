/* -- stap_shim.c TODO list --
 * This list establishes ownership of issues and work items.
 *
 * TODO(lc525):
 *  - fix need to reset relayfs channel on each write (stop resetting all values
 *  on each write (move to flight-record mode in relayfs)
 *  - test mmap interface for relayfs
 *  - provide sensible values for relayfs per-cpu buffer and sub-buffer sizes,
 *  in accordance to sizeof(struct accounting)
 *
 *  TODO(unowned):
 *  - memory management for more than one accounting struct (we'll need a whole
 *  vector of them
 *  - zero-copy to accounting-to-relayfs (why should we allocate things twice
 *  and then copy? - this is messy atm, especially because there is yet another
 *  copy in userspace)
 */
#include "res_kernel/stap_shim.h"


static struct rchan *chan;
static struct accounting *acct;
static int tid_acct;
static int acct_next_call;

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
    printk("Error doing a kmalloc\n");
    goto error;
  }

  if ( !(chan = relay_open(PROJECT_NAME, NULL, SUBBUF_SIZE,
         N_SUBBUFS, &relay_callbacks, NULL)) ) {
    kfree(acct);
    printk("Error doing a relay_open\n");
    goto error;
  }
  return 0;

error:
  printk("Error creating shared memory\n");
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

int _should_acct(int tid)
{
  return (acct_next_call == tid);
}

int _set_tid(int tid)
{
  debugk("_set_tid\n");
  tid_acct = tid;
  return 0;
}

int acct_next(int pid)
{
  acct_next_call = pid;
  return 0;
}
EXPORT_SYMBOL(acct_next);

int _clear_acct_next(void)
{
  acct_next_call = 0;
}
