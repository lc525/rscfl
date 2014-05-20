#include "/home/oc243/resourceful/src/kernel/resourceful.h"


static struct rchan *chan;
static struct accounting *acct;
static int tid_acct;
static int acct_next;

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
	if ( !(acct = ((struct accounting*) kmalloc(sizeof(struct accounting),
						    GFP_KERNEL))) ) {
		printk("Error doing a kmalloc\n");
		goto error;
	}

	if ( !(chan = relay_open(PROJNAME, NULL, SUBBUF_SIZE,
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
	acct->cpu.cycles = 0;
	return 0;
}

int _update_relay(void)
{
	relay_reset(chan);
	relay_write(chan, acct, sizeof(struct accounting));
	return 0;
}

int _should_acct(int tid)
{
	return ((tid == tid_acct) && acct_next);
}

int _set_tid(int tid)
{
	tid_acct = tid;
	return 0;
}
