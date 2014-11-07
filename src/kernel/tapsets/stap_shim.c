#include "rscfl/kernel/stap_shim.h"

#include <asm/atomic.h>
#include <linux/rwlock_types.h>
#include <linux/slab.h>
#include <linux/smp.h> // only here for smp_processor_id()
#include <linux/spinlock.h>

#include "rscfl/config.h"
#include "rscfl/costs.h"
#include "rscfl/res_common.h"
#include "rscfl/kernel/cpu.h"


long syscall_id_c;
syscall_acct_list_t *syscall_acct_list;

static rwlock_t lock = __RW_LOCK_UNLOCKED(lock);

int acct_next(pid_t pid, int syscall_nr)
{
  syscall_acct_list_t *to_acct =
      (syscall_acct_list_t *)kzalloc(sizeof(syscall_acct_list_t), GFP_KERNEL);
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
int _should_acct(pid_t pid, int syscall_nr, int probe_nest, const char *name,
                 struct accounting **acct)
{
  syscall_acct_list_t *e;
  struct accounting *acct_buf;
  pid_acct *current_pid_acct;
  rscfl_shared_mem_layout_t *rscfl_shared_mem;

  read_lock(&lock);
  current_pid_acct = CPU_VAR(current_acct);
  e = syscall_acct_list;
  while (e) {
    if ((e->pid == pid) &&
        ((syscall_nr == -1) || (e->syscall_nr == syscall_nr))) {
      if(current_pid_acct != NULL) {
        if (current_pid_acct->probe_data->syscall_acct) {
          *acct = current_pid_acct->probe_data->syscall_acct;
          read_unlock(&lock);
          return 1;
        }
        read_unlock(&lock);

        // Find a free struct accounting in the shared memory that we can
        // use.
        rscfl_shared_mem = current_pid_acct->shared_buf;
        acct_buf = rscfl_shared_mem->acct;
        BUG_ON(!acct_buf);
        while (acct_buf->in_use == 1) {
          debugk("in use: %p for (pid, id):(%d, %ld)\n", (void *)acct_buf,
              acct_buf->syscall_id.pid, acct_buf->syscall_id.id);
          acct_buf++;
          if ((void *)acct_buf + sizeof(struct accounting) >
              (void *)current_pid_acct->shared_buf->subsyses){
            acct_buf = current_pid_acct->shared_buf->acct;
            debugk("_should_acct: wraparound!<<<<<<<\n");
            break;
          }
        }
        // We have a free struct accounting now, so use it.
        current_pid_acct->probe_data->syscall_acct = acct_buf;
        current_pid_acct->probe_data->nest_level = 0;
        current_pid_acct->probe_data->real_call = 0;
        acct_buf->in_use = 1;
        acct_buf->syscall_id.pid = pid;
        acct_buf->syscall_id.id = e->syscall_nr;
        debugk("syscall_nr=%d\n", e->syscall_nr);
        // Initialise the subsys_accounting indices to -1, as they are used
        // to index an array, so 0 is valid.
        memset(acct_buf->acct_subsys, -1, sizeof(short) * NUM_SUBSYSTEMS);
        debugk("_should_acct %s: (yes, nr %d) %d, into %p\n", name,
            e->syscall_nr, pid, (void *)acct_buf);
        *acct = acct_buf;
        return 1;
      }
    }
    e = e->next;
  }
  read_unlock(&lock);
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

  pid_acct *current_pid_acct;

  int rc = -1;
  debugk("clear_acct_next %d\n", pid);

  read_lock(&lock);
  entry = syscall_acct_list;

  current_pid_acct = CPU_VAR(current_acct);
  current_pid_acct->probe_data->syscall_acct = NULL;

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
