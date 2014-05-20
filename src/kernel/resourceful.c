#include "resourceful.h"
#define MAX_NO_PIDS 10


struct rchan *chan;
struct accounting *acct;
int pids_acctd[MAX_NO_PIDS] = {0};

static struct dentry *create_buf_file_handler(const char *, struct dentry *,
                umode_t, struct rchan_buf *,
                int *);

static int remove_buf_file_handler(struct dentry *);

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

int _fill_struct()
{
  acct->cpu.cycles = 0;
  return 0;
}

int _update_relay(void)
{
  relay_write(chan, acct, sizeof(struct accounting));
  return 0;
}

int _should_acct(int pid)
{
  int *p = pids_acctd;
  while (!*p && (p - pids_acctd >= MAX_NO_PIDS)) {
    if (*p == pid)
      return 1;
  }
  return 0;
}

int _add_pid(int pid)
{
  int *p = pids_acctd;
  while (!*p)
    p++;
  if (p - pids_acctd >= MAX_NO_PIDS)
    return -1;
  *p = pid;
  return 0;
}



















