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

#define max(a,b)                                \
  ({ typeof (a) _a = (a);                       \
    typeof (b) _b = (b);                        \
    _a > _b ? _a : _b; })

#define min(a,b)                                \
  ({ typeof (a) _a = (a);                       \
    typeof (b) _b = (b);                        \
    _a > _b ? _b : _a; })

// macro function definitions
DEFINE_REDUCE_FUNCTION(rint, ru64)
DEFINE_REDUCE_FUNCTION(wc, struct timespec)

// define subsystem name array for user-space includes of subsys_list.h
const char *rscfl_subsys_name[NUM_SUBSYSTEMS] = {
    SUBSYS_TABLE(SUBSYS_AS_STR_ARRAY)
};

__thread rscfl_handle handle = NULL;

rscfl_handle rscfl_init_api(rscfl_version_t rscfl_ver)
{
  struct stat sb;
  void *ctrl, *buf;
  int fd_data, fd_ctrl;
  struct accounting acct;

  // library was compiled with RSCFL_VERSION, API called from rscfl_ver
  // emit warning if the APIs have different major versions
  if(RSCFL_VERSION.major != rscfl_ver.major) {
    fprintf(stderr, "rscfl: API major version mismatch: "
                    "%d (header) vs %d (library)\n",
                    rscfl_ver.major, RSCFL_VERSION.major);
    #ifdef RSCFL_ERR_VER_MISMATCH
      fprintf(stderr, "rscfl: initialisation aborted\n");
      return NULL;
    #endif
    // if ERROR_ON_VERSION_MISMATCH is not defined, we'll still try to
    // initialize rscfl
  }

  fd_data = open("/dev/" RSCFL_DATA_DRIVER, O_RDONLY);
  fd_ctrl = open("/dev/" RSCFL_CTRL_DRIVER, O_RDWR);
  rscfl_handle rhdl = (rscfl_handle)calloc(1, sizeof(*rhdl));
  if (!rhdl) {
    return NULL;
  }

  if ((fd_data == -1) || (fd_ctrl == -1)) {
    goto error;
  }

  // mmap memory to store our struct accountings, and struct subsys_accountings

  // note: this (data) mmap needs to happen _before_ the ctrl mmap because the
  // rscfl_data character device also does the initialisation of per-cpu
  // variables later used by rscfl_ctrl.
  buf = mmap(NULL, MMAP_BUF_SIZE, PROT_READ | PROT_WRITE,
             MAP_PRIVATE | MAP_POPULATE, fd_data, 0);
  if (buf == MAP_FAILED) {
    goto error;
  }
  rhdl->buf = buf;

  // mmap memory to store our interests.
  ctrl = mmap(NULL, MMAP_CTL_SIZE, PROT_READ | PROT_WRITE,
              MAP_PRIVATE | MAP_POPULATE, fd_ctrl, 0);
  if (ctrl == MAP_FAILED) {
    goto error;
  }

  // Initialise pointer so that we write our interests into the mmap-ed region
  // of memory, so resourceful can read them.
  rhdl->ctrl = ctrl;

  // Check data layout version
  if (rhdl->ctrl->version != rscfl_ver.data_layout) {
    fprintf(stderr,
            "rscfl: Version mismatch between rscfl API and kernel data layouts: %d (API) vs %d (.ko)\n",
            rscfl_ver.data_layout, rhdl->ctrl->version);
    goto error;
  }
  // Make an (accounted-for) system call to initialise the tokens.
  if (rscfl_acct_next(rhdl)) {
    goto error;
  };

  if ((close(fd_data) == -1) || (close(fd_ctrl) == -1)) {
    goto error;
  }
  if (rscfl_read_acct(rhdl, &acct)) {
    goto error;
  }

  return rhdl;

error:
  if (rhdl != NULL) {
    if (rhdl->buf != NULL) {
      munmap(rhdl->buf, MMAP_BUF_SIZE);
    }
    if (rhdl->ctrl != NULL) {
      munmap(rhdl->ctrl, MMAP_BUF_SIZE);
    }
    free(rhdl);
  }
  return NULL;
}

rscfl_handle rscfl_get_handle(void)
{
  if (handle == NULL) {
    handle = rscfl_init();
  }
  return handle;
}

int rscfl_get_token(rscfl_handle rhdl, rscfl_token_t **token)
{
  rscfl_token_list_t *token_list_hd;
  if ((rhdl == NULL) || (token == NULL)) {
    return -EINVAL;
  }
  // First see if we already have a token that we can reuse.
  if (rhdl->reuseable_tokens != NULL) {
    token_list_hd = rhdl->reuseable_tokens;
    *token = token_list_hd->token;
    (*token)->reset_count = 1;
    rhdl->reuseable_tokens = token_list_hd->next;
    free(token_list_hd);
  }
  // There are no reusable tokens. Get one of the freshly-baked tokens that
  // the rscfl kernel module has prepared for us.
  else if (rhdl->ready_token_sp) {
    rhdl->ready_token_sp--;
    *token = rhdl->fresh_tokens[rhdl->ready_token_sp];
    rhdl->fresh_tokens[rhdl->ready_token_sp]->reset_count = 1;
  } else {
  // There are no reusable tokens, and no freshly-baked tokens. The userspace
  // program needs to wait until the module creates more tokens.
    return -EAGAIN;
  }

  return 0;
}

int rscfl_free_token(rscfl_handle rhdl, rscfl_token_t *token)
{
  rscfl_token_list_t *new_hd;
  if ((rhdl == NULL) || (token == NULL)) {
    return -EINVAL;
  }
  new_hd = (rscfl_token_list_t *)malloc(sizeof(rscfl_token_list_t));
  if (new_hd == NULL) {
    return -ENOMEM;
  }
  new_hd->next = rhdl->reuseable_tokens;
  new_hd->token = token;
  rhdl->reuseable_tokens = new_hd;
  return 0;
}

int rscfl_acct_next_token(rscfl_handle rhdl, rscfl_token_t *token)
{
  syscall_interest_t *to_acct;
  if (rhdl == NULL) {
    return -EINVAL;
  }

  to_acct = &rhdl->ctrl->interest;
  to_acct->syscall_id = ++rhdl->lst_syscall.id;
  to_acct->syscall_nr = -1;
  rhdl->ctrl->num_new_tokens = NUM_READY_TOKENS - rhdl->ready_token_sp - 1;
  if (token != NULL) {
    to_acct->start_measurement = token->reset_count;
    token->reset_count = 0;
    to_acct->token = token->id;
  } else {
    to_acct->start_measurement = 1;
    to_acct->token = 0;
  }
  return 0;
}

int rscfl_read_acct(rscfl_handle rhdl, struct accounting *acct)
{
  int i = 0;
  if (rhdl == NULL) {
    return -EINVAL;
  }

  // See if we have any more fresh tokens to register in rhdl.

  struct rscfl_acct_layout_t *shared_mem =
      (struct rscfl_acct_layout_t *)rhdl->buf;
  struct rscfl_token *new_token;
  for (i = 0; i < NUM_READY_TOKENS; i++) {
    if (rhdl->ctrl->new_tokens[i]) {
      if (rhdl->ready_token_sp  < NUM_READY_TOKENS) {
        // Move the token onto the stack of fresh tokens.
        new_token = malloc(sizeof(struct rscfl_token));
        if (new_token == NULL) {
          return -ENOMEM;
        }

        new_token->id = rhdl->ctrl->new_tokens[i];
        rhdl->fresh_tokens[rhdl->ready_token_sp] = new_token;
        rhdl->ctrl->new_tokens[i] = 0;
        rhdl->ready_token_sp++;
      }

    }
  }

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

  if (acct == NULL) return NULL;

  ret_subsys_idx = malloc(sizeof(subsys_idx_set));
  if (!ret_subsys_idx) return NULL;

  ret_subsys_idx->set_size = acct->nr_subsystems;
  ret_subsys_idx->max_set_size = acct->nr_subsystems;
  ret_subsys_idx->set =
      malloc(sizeof(struct subsys_accounting) * acct->nr_subsystems);
  if (!ret_subsys_idx->set) {
    free(ret_subsys_idx);
    return NULL;
  }
  ret_subsys_idx->ids = malloc(sizeof(short) * acct->nr_subsystems);
  if (!ret_subsys_idx->ids) {
    free(ret_subsys_idx->set);
    free(ret_subsys_idx);
    return NULL;
  }

  for (i = 0; i < NUM_SUBSYSTEMS; ++i) {
    struct subsys_accounting *subsys = rscfl_get_subsys_by_id(rhdl, acct, i);
    if (subsys != NULL) {
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
  if (no_subsystems > NUM_SUBSYSTEMS) no_subsystems = NUM_SUBSYSTEMS;

  ret_subsys_idx = malloc(sizeof(subsys_idx_set));
  if (!ret_subsys_idx) return NULL;

  ret_subsys_idx->set_size = 0;
  ret_subsys_idx->max_set_size = no_subsystems;
  ret_subsys_idx->set = calloc(no_subsystems, sizeof(struct subsys_accounting));
  if (!ret_subsys_idx->set) {
    free(ret_subsys_idx);
    return NULL;
  }
  memset(ret_subsys_idx->idx, -1, sizeof(short) * NUM_SUBSYSTEMS);

  ret_subsys_idx->ids = malloc(sizeof(short) * no_subsystems);
  if (!ret_subsys_idx->ids) {
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
  if (!acct_from || !aggregator_into) return -EINVAL;

  curr_set_ix = aggregator_into->set_size;

  for (i = 0; i < NUM_SUBSYSTEMS; ++i) {
    struct subsys_accounting *new_subsys =
        rscfl_get_subsys_by_id(rhdl, acct_from, i);
    if (new_subsys != NULL) {
      if (aggregator_into->idx[i] == -1) {
        // new_subsys i not in aggregator_into, add if sufficient space
        if (curr_set_ix < aggregator_into->max_set_size) {
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

void free_subsys_idx_set(subsys_idx_set *subsys_set)
{
  if (subsys_set != NULL) {
    free(subsys_set->set);
    free(subsys_set->ids);
  }
  free(subsys_set);
}

inline void rscfl_subsys_merge(struct subsys_accounting *e,
                               const struct subsys_accounting *c) {
  e->subsys_entries              += c->subsys_entries;
  e->subsys_exits                += c->subsys_exits;

  e->cpu.cycles                  += c->cpu.cycles;
  e->cpu.branch_mispredictions   += c->cpu.branch_mispredictions;
  e->cpu.instructions            += c->cpu.instructions;

  rscfl_timespec_add(&e->cpu.wall_clock_time, &c->cpu.wall_clock_time);

  e->mem.alloc                   += c->mem.alloc;
  e->mem.freed                   += c->mem.freed;
  e->mem.page_faults             += c->mem.page_faults;
  e->mem.align_faults            += c->mem.align_faults;

  rscfl_timespec_add(&e->sched.wct_out_local, &c->sched.wct_out_local);
  rscfl_timespec_add(&e->sched.xen_sched_wct, &c->sched.xen_sched_wct);

  e->sched.xen_schedules           += c->sched.xen_schedules;
  e->sched.xen_sched_cycles        += c->sched.xen_sched_cycles;
  e->sched.xen_blocks              += c->sched.xen_blocks;
  e->sched.xen_yields              += c->sched.xen_yields;
  e->sched.xen_evtchn_pending_size += c->sched.xen_evtchn_pending_size;
  e->sched.xen_credits_min = min(e->sched.xen_credits_min,
                                 c->sched.xen_credits_min);
  e->sched.xen_credits_max = max(e->sched.xen_credits_max,
                                 c->sched.xen_credits_max);
}

struct subsys_accounting* rscfl_get_subsys_by_id(rscfl_handle rhdl,
                                                 struct accounting *acct,
                                                 rscfl_subsys subsys_id)
{
  if (!acct || acct->acct_subsys[subsys_id] == -1) {
    return NULL;
  }
  rscfl_acct_layout_t *rscfl_data = (rscfl_acct_layout_t *)rhdl->buf;
  return &rscfl_data->subsyses[acct->acct_subsys[subsys_id]];
}

void rscfl_subsys_free(rscfl_handle rhdl, struct accounting *acct)
{
  int i;
  if (rhdl == NULL || acct == NULL) return;

  for (i = 0; i < NUM_SUBSYSTEMS; ++i) {
    struct subsys_accounting *subsys = rscfl_get_subsys_by_id(rhdl, acct, i);
    if (subsys != NULL) subsys->in_use = 0;
  }
}

// Shadow kernels.

void rscfl_spawn_shdw(rscfl_handle rhdl)
{
  rhdl->ctrl->interest.shdw_operation = SPAWN_ONLY;
}

void rscfl_set_num_shdw_pages(rscfl_handle rhdl, int pages)
{
  rhdl->ctrl->interest.shdw_pages = pages;
}

void rscfl_spawn_shdw_for_pid(rscfl_handle rhdl)
{
  rhdl->ctrl->interest.shdw_operation = SPAWN_SWAP_ON_SCHED;
}

void rscfl_in_shdw_pages(rscfl_handle rhdl, int use_shdw, int shdw_pages)
{
  rhdl->ctrl->interest.use_shdw = use_shdw;
  rhdl->ctrl->interest.shdw_pages = shdw_pages;
}
