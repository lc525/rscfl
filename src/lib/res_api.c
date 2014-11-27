/* Resourceful user-facing API
 * Usage example:
 *
 * rscfl_init(); // called on each thread - allocates two shared memory regions
 *               // between user and kernel space: one that holds resource
 *               // accounting data and a second for storing expressions of
 *               // interest
 * int filter = SYS | PROC | NET_SOCK; // defines what resources we're
 *                                     // interested in, default: ALL (include
 *                                     // all resources)
 * // declare interest in measuring the resource consumption of the next
 * // syscall by calling rscfl_acct_next
 * call_cost* cost_o = rscfl_acct_next(filter);
 * int fd = open("/../file_path", O_CREAT); // syscall being measured
 *
 * call_cost* cost_w = acct_next(filter);
 * int res = write(fd, &buf, BUF_SIZE);
 *
 * // if the write is asynchronous, cost_w will keep being updated by the
 * // kernel for a while
 *
 * // do whatever you want with the call_cost data. you can read the sync
 * // component as soon as the syscall is done, but you should touch the async
 * // component only when the kernel has set the async_done flag to true
 * // you can register a callback for when that happens with:
 * cost_callback_async(cost_w, callback_function);
 *
 * //...und so weiter
 *
 * fini_resource_acct(); // de-allocate resource accounting structures
 *
 */
#include "rscfl/user/res_api.h"

#include <errno.h>
#include <fcntl.h>
#include <linux/netlink.h>
#include <linux/types.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/syscall.h>
#include <unistd.h>

#include "rscfl/config.h"
#include "rscfl/costs.h"
#include "rscfl/res_common.h"
#include "rscfl/kernel/stap_shim.h"

// define subsystem name array for user-space includes of subsys_list.h
const char *rscfl_subsys_name[NUM_SUBSYSTEMS] = {
    SUBSYS_TABLE(SUBSYS_AS_STR_ARRAY)
};

// THIS IS HERE ONLY FOR THE HOTDEP PAPER
// TODO(lc525): remove
__thread rscfl_handle handle = NULL;

rscfl_handle rscfl_init()
{
  struct stat sb;
  void *ctrl;
  int fd_data = open("/dev/" RSCFL_DATA_DRIVER, O_RDONLY);
  int fd_ctrl = open("/dev/" RSCFL_CTRL_DRIVER, O_RDWR);
  rscfl_handle rhdl = (rscfl_handle)malloc(sizeof(*rhdl));
  if (!rhdl) {
    return NULL;
  }

  if ((fd_data == -1) || (fd_ctrl == -1)) {
    goto error;
  }

  // mmap memory to store our struct accountings, and struct subsys_accountings.
  rhdl->buf = mmap(NULL, MMAP_BUF_SIZE, PROT_READ | PROT_WRITE,
                   MAP_PRIVATE | MAP_POPULATE, fd_data, 0);
  if (rhdl->buf == MAP_FAILED) {
    goto error;
  }

  // mmap memory to store our interests.
  ctrl = mmap(NULL, MMAP_CTL_SIZE, PROT_READ | PROT_WRITE,
              MAP_PRIVATE | MAP_POPULATE, fd_ctrl, 0);
  if (ctrl == MAP_FAILED) {
    goto error;
  }

  // Initialise pointer so that we write our interests into the mmap-ed region
  // of memory, so resourceful can read them.
  rhdl->interests = ctrl;

  if ((close(fd_data) == -1) || (close(fd_ctrl) == -1)) {
    goto error;
  }

  rhdl->lst_syscall.id = 0;
  return rhdl;

error:
  if (rhdl != NULL) {
    if (rhdl->buf != NULL) {
      munmap(rhdl->buf, MMAP_BUF_SIZE);
    }
    if (rhdl->interests != NULL) {
      munmap(rhdl->interests, MMAP_BUF_SIZE);
    }
    free(rhdl);
  }
  return NULL;
}

rscfl_handle rscfl_get_handle()
{
  if (handle == NULL) {
    handle = rscfl_init();
  }
  return handle;
}

int rscfl_acct_next(rscfl_handle rhdl)
{
  int rc;
  syscall_interest_t *to_acct;
  if (rhdl == NULL) {
    return -EINVAL;
  }

  to_acct = rhdl->interests;
  to_acct->syscall_id = ++rhdl->lst_syscall.id;
  to_acct->syscall_nr = -1;

  return 0;
}

int rscfl_read_acct(rscfl_handle rhdl, struct accounting *acct)
{
  int i = 0;
  if (rhdl == NULL) return -1;

  struct accounting *shared_acct = (struct accounting *)rhdl->buf;
  if (shared_acct != NULL) {
    while (i < STRUCT_ACCT_NUM) {
      if (shared_acct->in_use == 1) {
        if (shared_acct->syscall_id.id == rhdl->lst_syscall.id) {
          memcpy(acct, shared_acct, sizeof(struct accounting));
          shared_acct->in_use = 0;
          //TODO(lc525) shouldn't we mark all subsystems as "-1" as well?
          return shared_acct->rc;
        } else {
          shared_acct++;
          i++;
        }
      } else {
        shared_acct++;
        i++;
      }
    }
  } else {
    return -EINVAL;
  }
  return -EINVAL;
}

/*!
 * \brief merge the data measured in two kernel subsystems
 *
 * \param [in,out] e the subsys_accounting in which data will be aggregated
 *                   (the existing subsys_accounting)
 * \param [in]     c the current subsys_accounting. The measurement data for
 *                   this subsystem will be added to the exising
 *                   subsys_accounting
 */
inline void rscfl_subsys_merge(struct subsys_accounting *e,
                               struct subsys_accounting *c) {
  e->cpu.cycles                  += c->cpu.cycles;
  e->cpu.branch_mispredictions   += c->cpu.branch_mispredictions;
  e->cpu.instructions            += c->cpu.instructions;

  e->cpu.wall_clock_time.tv_sec  += c->cpu.wall_clock_time.tv_sec;
  e->cpu.wall_clock_time.tv_nsec += c->cpu.wall_clock_time.tv_nsec;
  if(e->cpu.wall_clock_time.tv_nsec > 1e9) {
    e->cpu.wall_clock_time.tv_nsec -= 1e9;
    e->cpu.wall_clock_time.tv_sec++;
  }

  e->mem.alloc                   += c->mem.alloc;
  e->mem.freed                   += c->mem.freed;
  e->mem.page_faults             += c->mem.page_faults;
  e->mem.align_faults            += c->mem.align_faults;
}


/*!
 * \brief gets the measurements done for acct in a particular kernel subsystem
 *
 * \param rhdl the resourceful handle for the thread where measurement is done
 *             (typically the current thread)
 * \param [in] acct a pointer to the accounting data structure obtained from
 *                  calling rscfl_acct_read
 * \param subsys_id the id of the subsystem (one of the values in the
 *                  rscfl_subsys enum)
 *
 * returns NULL if the measured code path did not touch subsystem subsys_id
 */
struct subsys_accounting* rscfl_get_subsys_by_id(rscfl_handle rhdl,
                                                 struct accounting *acct,
                                                 rscfl_subsys subsys_id)
{
  if (!acct || acct->acct_subsys[subsys_id] == -1) {
    return NULL;
  }
  rscfl_shared_mem_layout_t *rscfl_data = (rscfl_shared_mem_layout_t*)rhdl->buf;
  if (!rscfl_data) {
    return NULL;
  }
  return &rscfl_data->subsyses[acct->acct_subsys[subsys_id]];
}

/*!
 * \brief marks the kernel-side memory used for subsystem accounting storage as
 *        free
 *
 *  The memory for all subsystems touched during measurements done for acct is
 *  marked as available.
 */
void rscfl_subsys_free(rscfl_handle rhdl, struct accounting *acct)
{
  int i;
  if(acct == NULL) return;

  for(i = 0; i < NUM_SUBSYSTEMS; ++i) {
    struct subsys_accounting *subsys = rscfl_get_subsys_by_id(rhdl, acct, i);
    if(subsys != NULL) subsys->in_use = 0;
  }
}


/*!
 * \brief reads the per-subsystem resource accounting that was measured for acct
 *
 * It returns a subsys_idx_set structure, containing both the set of subsystems
 * that were active for the measured system call(s), and an subsystem_id-based
 * index for fast querying.
 *
 * The resource accounting data is copied to userspace, freeing the
 * corresponding kernel resources
 *
 * \param rhdl the resourceful handle for the thread where measurement is done
 *             (typically the current thread)
 * \param [in] acct a pointer to the accounting data structure obtained from
 *                  calling rscfl_acct_read
 */
subsys_idx_set* rscfl_get_subsys(rscfl_handle rhdl, struct accounting *acct)
{
  int curr_set_ix = 0, i;
  subsys_idx_set *ret_subsys_idx;

  if(acct == NULL) return NULL;

  ret_subsys_idx = malloc(sizeof(subsys_idx_set));
  if(!ret_subsys_idx) return NULL;


  ret_subsys_idx->set_size = acct->nr_subsystems;
  ret_subsys_idx->max_set_size = acct->nr_subsystems;
  ret_subsys_idx->set =
    malloc(sizeof(struct subsys_accounting) * acct->nr_subsystems);
  if(!ret_subsys_idx->set) {
    free(ret_subsys_idx);
    return NULL;
  }

  for(i = 0; i < NUM_SUBSYSTEMS; ++i) {
    struct subsys_accounting *subsys = rscfl_get_subsys_by_id(rhdl, acct, i);
    if(subsys != NULL) {
      ret_subsys_idx->idx[i] = curr_set_ix;
      memcpy(&ret_subsys_idx->set[curr_set_ix], subsys,
             sizeof(struct subsys_accounting));
      subsys->in_use = 0;
      curr_set_ix++;
    } else {
      ret_subsys_idx->idx[i] = -1;
    }
  }

  return ret_subsys_idx;
}

/*!
 * \brief returns an empty subsys_idx_set capable of holding resource accounting
 *        data for no_subsystems.
 *
 * This can be used when aggregating data across multiple acct_next calls, by
 * passing the resulting subsys_idx_set to rscfl_merge_acct_into.
 *
 * \param no_subsystems the maximum number of subsystems which the aggregator
 *                      will be able to hold. Calling this with NUM_SUBSYSTEMS
 *                      means the aggregator can hold everything (safest option
 *                      but it also allocates quite a bit more memory)
 */
subsys_idx_set* rscfl_get_new_aggregator(unsigned short no_subsystems)
{
  subsys_idx_set *ret_subsys_idx;
  if(no_subsystems > NUM_SUBSYSTEMS) no_subsystems = NUM_SUBSYSTEMS;

  ret_subsys_idx = malloc(sizeof(subsys_idx_set));
  if(!ret_subsys_idx) return NULL;

  ret_subsys_idx->set_size = 0;
  ret_subsys_idx->max_set_size = no_subsystems;
  ret_subsys_idx->set =
    calloc(no_subsystems, sizeof(struct subsys_accounting));
  if(!ret_subsys_idx->set) {
    free(ret_subsys_idx);
    return NULL;
  }
  memset(ret_subsys_idx->idx, -1, sizeof(short) * NUM_SUBSYSTEMS);

  return ret_subsys_idx;
}


/*!
 * \brief rscfl_merge_acct_into allows fast aggregation of subsys accounting
 *        data in user space
 *
 * \param rhdl the resourceful handle for the thread where measurement is done
 *             (typically the current thread)
 * \param [in] acct_from the acct data structure from where we want to take the
 *                       per-subsystem information
 * \param [in] aggregator_into the subsys_idx_set aggregator to which we'll add
 *                             the data measured for acct_from.
 *
 * If acct_from has touched subsystems not present in subsys_idx_set before,
 * those will be added to the aggregator (a set union of the subsystems is
 * performed).
 *
 * If the aggregator is not large enough to hold the union, the
 * number of subsystems that couldn't be added is returned. 0 is returned on
 * normal exit.
 *
 * This function frees the kernel-side resources allocated for the subsystems
 * that we have aggregated. If aggregator_into already contains data for
 * a particular subsystem, no extra copies of the new subsystem data are done
 * in user-space.
 */
int rscfl_merge_acct_into(rscfl_handle rhdl, struct accounting *acct_from,
                          subsys_idx_set *aggregator_into)
{
  int curr_set_ix, i, rc = 0;
  if(!acct_from || !aggregator_into) return -1;

  curr_set_ix = aggregator_into->set_size;

  for(i = 0; i < NUM_SUBSYSTEMS; ++i) {
    struct subsys_accounting *new_subsys =
      rscfl_get_subsys_by_id(rhdl, acct_from, i);
    if(new_subsys != NULL) {
      if(aggregator_into->idx[i] == -1) {
        // new_subsys i not in aggregator_into, add if sufficient space
        if(curr_set_ix < aggregator_into->max_set_size) {
          aggregator_into->idx[i] = curr_set_ix;
          memcpy(&aggregator_into->set[curr_set_ix], new_subsys,
              sizeof(struct subsys_accounting));
          new_subsys->in_use = 0;
          curr_set_ix++;
          aggregator_into->set_size++;
        } else {
          // not enough space in aggregator_into, set error but continue
          // (the values that could be aggregated remain correct)
          rc++;
        }
      } else {
        // subsys i exists, merge values
        rscfl_subsys_merge(&aggregator_into->set[aggregator_into->idx[i]],
                           new_subsys);
        new_subsys->in_use = 0;
      }
    }
  }
  return rc;
}

