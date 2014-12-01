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

// macro function definitions
DEFINE_REDUCE_FUNCTION(rint, ru64)

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
  ret_subsys_idx->ids = malloc(sizeof(short) * acct->nr_subsystems);
  if(!ret_subsys_idx->ids) {
    free(ret_subsys_idx->set);
    free(ret_subsys_idx);
    return NULL;
  }

  for(i = 0; i < NUM_SUBSYSTEMS; ++i) {
    struct subsys_accounting *subsys = rscfl_get_subsys_by_id(rhdl, acct, i);
    if(subsys != NULL) {
      ret_subsys_idx->idx[i] = curr_set_ix;
      memcpy(&ret_subsys_idx->set[curr_set_ix], subsys,
             sizeof(struct subsys_accounting));
      ret_subsys_idx->ids[curr_set_ix] = i;
      subsys->in_use = 0;
      curr_set_ix++;
    } else {
      ret_subsys_idx->idx[i] = -1;
    }
  }

  return ret_subsys_idx;
}

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

  ret_subsys_idx->ids = malloc(sizeof(short) * no_subsystems);
  if(!ret_subsys_idx->ids) {
    free(ret_subsys_idx->set);
    free(ret_subsys_idx);
    return NULL;
  }

  return ret_subsys_idx;
}

int rscfl_merge_acct_into(rscfl_handle rhdl, struct accounting *acct_from,
                          subsys_idx_set *aggregator_into)
{
  int curr_set_ix, i, rc = 0;
  if(!acct_from || !aggregator_into) return -EINVAL;

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
          aggregator_into->ids[curr_set_ix] = i;
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

void free_subsys_idx_set(subsys_idx_set *subsys_set) {
  if(subsys_set != NULL){
    free(subsys_set->set);
    free(subsys_set->ids);
  }
  free(subsys_set);
}



inline void rscfl_subsys_merge(struct subsys_accounting *e,
                               const struct subsys_accounting *c) {
  e->cpu.cycles                  += c->cpu.cycles;
  e->cpu.branch_mispredictions   += c->cpu.branch_mispredictions;
  e->cpu.instructions            += c->cpu.instructions;

  timespec_add(&(e->cpu.wall_clock_time), &(c->cpu.wall_clock_time));

  e->mem.alloc                   += c->mem.alloc;
  e->mem.freed                   += c->mem.freed;
  e->mem.page_faults             += c->mem.page_faults;
  e->mem.align_faults            += c->mem.align_faults;
}

inline void timespec_add(struct timespec *to, const struct timespec *from) {
  to->tv_sec += from->tv_sec;
  to->tv_nsec += from->tv_nsec;

  // 1s = 1e9 ns; if tv_nsec is above 1s, then we have to add that to the
  // seconds field (tv_sec) and reduce tv_nsec accordingly
  if(to->tv_nsec > 1e9) {
    to->tv_nsec -= 1e9;
    to->tv_sec++;
  }

}

struct subsys_accounting* rscfl_get_subsys_by_id(rscfl_handle rhdl,
                                                 struct accounting *acct,
                                                 rscfl_subsys subsys_id)
{
  if (!acct || acct->acct_subsys[subsys_id] == -1) {
    return NULL;
  }
  rscfl_shared_mem_layout_t *rscfl_data = (rscfl_shared_mem_layout_t*)rhdl->buf;
  return &rscfl_data->subsyses[acct->acct_subsys[subsys_id]];
}

void rscfl_subsys_free(rscfl_handle rhdl, struct accounting *acct)
{
  int i;
  if(acct == NULL) return;

  for(i = 0; i < NUM_SUBSYSTEMS; ++i) {
    struct subsys_accounting *subsys = rscfl_get_subsys_by_id(rhdl, acct, i);
    if(subsys != NULL) subsys->in_use = 0;
  }
}

