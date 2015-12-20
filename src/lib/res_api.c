#include "rscfl/user/res_api.h"

#include <errno.h>
#include <fcntl.h>
#include <linux/netlink.h>
#include <linux/types.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
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

#ifdef RSCFL_BENCH
syscall_interest_t dummy_interest;
#endif

rscfl_handle rscfl_init_api(rscfl_version_t rscfl_ver, rscfl_config* config)
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
    fprintf(stderr, "Unable to allocate memory for rscfl handle\n");
    return NULL;
  }

  if ((fd_data == -1) || (fd_ctrl == -1)) {
    fprintf(stderr, "rscfl:Unable to access data or ctrl devices\n");
    goto error;
  }
  rhdl->fd_ctrl = fd_ctrl;

  if(config != NULL) {
    ioctl(rhdl->fd_ctrl, RSCFL_CONFIG_CMD, config);
  }

  // mmap memory to store our struct accountings, and struct subsys_accountings

  // note: this (data) mmap needs to happen _before_ the ctrl mmap because the
  // rscfl_data character device also does the initialisation of per-cpu
  // variables later used by rscfl_ctrl.
  buf = mmap(NULL, MMAP_BUF_SIZE, PROT_READ | PROT_WRITE,
             MAP_PRIVATE | MAP_POPULATE, fd_data, 0);
  if (buf == MAP_FAILED) {
    fprintf(stderr,
	    "rscfl: Unable to mmap shared memory with kernel module for data.\n");
    goto error;
  }
  rhdl->buf = buf;

  // mmap memory to store our interests.
  ctrl = mmap(NULL, MMAP_CTL_SIZE, PROT_READ | PROT_WRITE,
              MAP_PRIVATE | MAP_POPULATE, fd_ctrl, 0);
  if (ctrl == MAP_FAILED) {
    fprintf(stderr,
	    "rscfl: Unable to mmap shared memory for storing interests\n");
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

  rhdl->lst_syscall_id = RSCFL_SYSCALL_ID_OFFSET;
  handle = rhdl;
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

rscfl_handle rscfl_get_handle_api(rscfl_config *cfg)
{
  if (handle == NULL) {
    handle = rscfl_init(cfg);
  }
  return handle;
}

int rscfl_get_token(rscfl_handle rhdl, rscfl_token **token)
{
  _Bool consume_ctrl = 0;
  rscfl_token_list *token_list_hd;
  if ((rhdl == NULL) || (token == NULL)) {
    return -EINVAL;
  }
  // First see if there are any available tokens in the free list
  if(rhdl->free_token_list != NULL) {
    token_list_hd = rhdl->free_token_list;
    *token = token_list_hd->token;
    (*token)->first_acct = 1;
    rhdl->free_token_list = token_list_hd->next;
    free(token_list_hd);
    return 0;
  } else if(rhdl->ctrl->num_avail_token_ids > 0){
    // consume tokens registered by the kernel with the rscfl_ctrl device
    consume_ctrl = 1;
  } else {
    // explicitly request from the rscfl kernel module some more tokens
    int rc;
    rc = ioctl(rhdl->fd_ctrl, RSCFL_NEW_TOKENS_CMD);
    if(rc != 0)
      return -EAGAIN;
    else
      consume_ctrl = 1;
  }

  // the kernel has placed available token ids in rhdl->ctrl->avail_token_ids
  // effectively, the kernel promisses not to use those ids for any other
  // resource accounting activities.
  //
  // create user-space tokens for those kernel-side ids and add them to a free
  // token list
  //
  // skip adding the first of the available kernel tokens to the free list;
  // instead, return it as the out-argument of this function (**token)
  if(consume_ctrl && rhdl->ctrl->num_avail_token_ids > 0) {
    int i;
    for(i=0; i<rhdl->ctrl->num_avail_token_ids; i++) {
      rscfl_token *new_token = (rscfl_token *)malloc(sizeof(rscfl_token));
      new_token->id = rhdl->ctrl->avail_token_ids[i];
      new_token->first_acct = 1;
      rhdl->ctrl->avail_token_ids[i] = NO_TOKEN;
      if(i == 0) {
        *token = new_token;
      } else {
        rscfl_token_list *new_free_token = (rscfl_token_list *)malloc(sizeof(rscfl_token_list));
        new_free_token->token = new_token;
        new_free_token->next = rhdl->free_token_list;
        rhdl->free_token_list = new_free_token;
      }
    }
    rhdl->ctrl->num_avail_token_ids = 0;
  } else {
    return -EAGAIN;
  }

  return 0;
}

int rscfl_switch_token(rscfl_handle rhdl, rscfl_token *token_to){
  unsigned short new_id;
  if(token_to == NULL) {
    new_id = NO_TOKEN;
  } else {
    new_id = token_to->id;
  }
  if(rhdl->ctrl->interest.token_id != new_id){
    printf("RSCFL SWAP: old_tk_id:%d, new_tk_id:%d\n", rhdl->ctrl->interest.token_id, new_id);
    rhdl->ctrl->interest.token_id = new_id;
    rhdl->ctrl->interest.token_swapped = 1;
    return 0;
  } else {
    return 1; // no switch necessary, token_to already active
  }
}

int rscfl_free_token(rscfl_handle rhdl, rscfl_token *token)
{
  rscfl_token_list *new_hd;
  if ((rhdl == NULL) || (token == NULL)) {
    return -EINVAL;
  }
  printf("FREE TK_id:%d\n", token->id);
  new_hd = (rscfl_token_list *)malloc(sizeof(rscfl_token_list));
  if (new_hd == NULL) {
    return -ENOMEM;
  }
  new_hd->next = rhdl->free_token_list;
  new_hd->token = token;
  rhdl->free_token_list = new_hd;
  return 0;
}

int rscfl_acct_api(rscfl_handle rhdl, rscfl_token *token, interest_flags fl)
{
  syscall_interest_t *to_acct;
  int old_token_id;
  _Bool rst;
  if (rhdl == NULL) {
    return -EINVAL;
  }
#ifdef RSCFL_BENCH
  if(likely((fl & IST_KNOP) == 0))
    to_acct = &rhdl->ctrl->interest;
  else
    to_acct = &dummy_interest;
#else
  to_acct = &rhdl->ctrl->interest;
#endif
  to_acct->flags = fl;
  old_token_id = to_acct->token_id;
  printf("RSCFL ACCT: old_tk_id:%d, new_tk_id:%d\n", old_token_id, token->id);

  if((fl & IST_START) != 0) {
    to_acct->syscall_id = ID_RSCFL_IGNORE;
  }
  else if((fl & IST_STOP) != 0) {
    to_acct->syscall_id = 0;
    return 0;
  } else {
    to_acct->syscall_id = ++rhdl->lst_syscall_id;
  }
  rst = ((fl & IST_RESET) != 0);

  if (token != NULL) {
    to_acct->first_measurement = token->first_acct | rst;
    token->first_acct = 0;
    to_acct->token_id = token->id;
  } else {
    if((fl & IST_NEXT) != 0)
      to_acct->first_measurement = 1;
    else
      to_acct->first_measurement = rst;
    to_acct->token_id = NO_TOKEN;
  }

  if(old_token_id != to_acct->token_id) to_acct->token_swapped = 1;
  return 0;
}

int rscfl_read_acct_api(rscfl_handle rhdl, struct accounting *acct, rscfl_token *token)
{
  int i = 0;
  unsigned short tk_id;
  if (rhdl == NULL) {
    return -EINVAL;
  }

  if(token == NULL) {
    tk_id = rhdl->ctrl->interest.token_id;
  }
  else
    tk_id = token->id;

  struct accounting *shared_acct = (struct accounting *)rhdl->buf;
  printf("READ for: tk_id=%d\n", shared_acct->token_id);
  if (shared_acct != NULL) {
    while (i < STRUCT_ACCT_NUM) {
      if (shared_acct->in_use == 1) {
        printf("\tid=%lu, tk_id=%d\n", shared_acct->syscall_id, shared_acct->token_id);
        if ((shared_acct->syscall_id == rhdl->lst_syscall_id) ||
            (shared_acct->syscall_id == ID_RSCFL_IGNORE && shared_acct->token_id == tk_id)) {
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

int rscfl_getreset_probe_exits(rscfl_handle rhdl) {
  int exits;
  rscfl_acct_layout_t *rscfl_data = (rscfl_acct_layout_t *)rhdl->buf;
  exits = rscfl_data->subsys_exits;
  rscfl_data->subsys_exits = 0;
  return exits;
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

  e->sched.run_delay               += c->sched.run_delay;
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
#if SHDW_ENABLED != 0
int rscfl_spawn_shdw(rscfl_handle rhdl, shdw_hdl *hdl)
{
  int rc;
  rscfl_ioctl_t ioctl_arg = {0};
  ioctl_arg.shdw_operation = SPAWN_ONLY;
  rc = ioctl(rhdl->fd_ctrl, RSCFL_SHDW_CMD, &ioctl_arg);
  *hdl = ioctl_arg.new_shdw_id;
  return rc;
}

int rscfl_spawn_shdw_for_pid(rscfl_handle rhdl)
{
  rscfl_ioctl_t ioctl_arg = {0};
  ioctl_arg.shdw_operation = SPAWN_SWAP_ON_SCHED;
  ioctl_arg.num_shdw_pages = -1;
  return ioctl(rhdl->fd_ctrl, RSCFL_SHDW_CMD, &ioctl_arg);
}

int rscfl_use_shdw_pages(rscfl_handle rhdl, int use_shdw, int shdw_pages)
{
  rscfl_ioctl_t ioctl_arg = {0};
  ioctl_arg.shdw_operation = SWAP;
  ioctl_arg.swap_to_shdw = use_shdw;
  ioctl_arg.num_shdw_pages = shdw_pages;
  return ioctl(rhdl->fd_ctrl, RSCFL_SHDW_CMD, &ioctl_arg);
}
#endif /* SHDW_ENABLED */
