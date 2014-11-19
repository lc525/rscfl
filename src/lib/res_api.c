#include "rscfl/user/res_api.h"

#include <errno.h>
#include <fcntl.h>
#include <linux/netlink.h>
#include <linux/types.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>  // malloc builtin; avoids debug compilation warning
#include <string.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/syscall.h>
#include <unistd.h>

#include "rscfl/config.h"
#include "rscfl/costs.h"
#include "rscfl/res_common.h"
#include "rscfl/kernel/stap_shim.h"

static struct sockaddr_nl src_addr, dest_addr;
static struct nlmsghdr *nlh = NULL;
static struct iovec iov;
static int sock_fd;
static struct msghdr msg;

// define subsystem name array for user-space includes of subsys_list.h
const char *rscfl_subsys_name[] = {
    SUBSYS_TABLE(SUBSYS_AS_STR_ARRAY)
};

// THIS IS HERE ONLY FOR THE HOTDEP PAPER
// TODO(lc525): remove
__thread rscfl_handle handle = NULL;

rscfl_handle rscfl_init()
{
  struct stat sb;
  int fd = open("/dev/" RSCFL_DRIVER, O_RDWR);
  rscfl_handle rhdl = (rscfl_handle)malloc(sizeof(*rhdl));
  if (!rhdl) {
    return NULL;
  }

  if (fd == -1) {
    goto error;
  }

  // mmap a chunk of data the size of all of the sub-buffers (def in config.h)
  rhdl->buf =
      mmap(NULL, MMAP_BUF_SIZE, PROT_READ | PROT_WRITE, MAP_PRIVATE, fd, 0);
  if (rhdl == MAP_FAILED) {
    goto error;
  }

  // Return the fd to the system
  if (close(fd) == -1) {
    goto error;
  }

  rhdl->lst_syscall.id = 0;

  return rhdl;

error:
  if (rhdl) {
    if (rhdl->buf) {
      munmap(rhdl->buf, MMAP_BUF_SIZE);
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
  if (rhdl == NULL) return -1;

  int rc;
  sock_fd = socket(PF_NETLINK, SOCK_RAW, NETLINK_USER);
  if (sock_fd < 0) {
    return -1;
  }

  memset(&src_addr, 0, sizeof(src_addr));
  src_addr.nl_family = AF_NETLINK;
  src_addr.nl_pid = (long int)syscall(__NR_gettid); /* self pid */

  if ((rc = bind(sock_fd, (struct sockaddr *)&src_addr, sizeof(src_addr)))) {
    return rc;
  }

  memset(&dest_addr, 0, sizeof(dest_addr));
  memset(&dest_addr, 0, sizeof(dest_addr));
  dest_addr.nl_family = AF_NETLINK;
  dest_addr.nl_pid = 0;    /* For Linux Kernel */
  dest_addr.nl_groups = 0; /* unicast */

  nlh = (struct nlmsghdr *)malloc(NLMSG_SPACE(MAX_PAYLOAD));
  memset(nlh, 0, NLMSG_SPACE(MAX_PAYLOAD));
  nlh->nlmsg_len = NLMSG_SPACE(MAX_PAYLOAD);
  nlh->nlmsg_pid = getpid();
  nlh->nlmsg_flags = 0;

  memcpy(NLMSG_DATA(nlh), &rhdl->lst_syscall, sizeof(rscfl_syscall_id_t));

  iov.iov_base = (void *)nlh;
  iov.iov_len = nlh->nlmsg_len;
  msg.msg_name = (void *)&dest_addr;
  msg.msg_namelen = sizeof(dest_addr);
  msg.msg_iov = &iov;
  msg.msg_iovlen = 1;

  sendmsg(sock_fd, &msg, 0);

  close(sock_fd);

  rhdl->lst_syscall.id++;

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
        if (shared_acct->syscall_id.id == (rhdl->lst_syscall.id - 1)) {
          memcpy(acct, shared_acct, sizeof(struct accounting));
          shared_acct->in_use = 0;
          return 0;
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

/*
 * Given a userspace rscfl handle (rhdl), add to its struct accounting the
 * costs that the kernel has measured in acct.
 *
 * Walk through all of the struct accountings that exist in the current
 * rscfl_handle. For each struct accounting in rscfl, we check to see if its
 * syscall_id is the same as that in acct. If so, we add the costs of that
 * struct accounting to what is already in
 */
int rscfl_merge_acct(rscfl_handle rhdl, struct accounting *acct)
{
  int i = 0;
  int subsys_no;

  // aliases into rhdl's and accts subsys_accountings.
  struct subsys_accounting *acct_subsys;
  struct subsys_accounting *rhdl_subsys;

  if (rhdl == NULL) return -1;

  rscfl_shared_mem_layout_t *rscfl_data =
      (rscfl_shared_mem_layout_t *)rhdl->buf;

  struct accounting *rhdl_acct = rscfl_data->acct;
  if (rhdl_acct != NULL) {
    while (i < STRUCT_ACCT_NUM) {
      if (rhdl_acct->in_use) {
        if (rhdl_acct->syscall_id.id == (rhdl->lst_syscall.id - 1)) {
          // Sum together accounting values in each subsystem that they're set.
          for (subsys_no = 0; subsys_no < NUM_SUBSYSTEMS; subsys_no++) {
            acct_subsys = &rscfl_data->subsyses[acct->acct_subsys[subsys_no]];
            rhdl_subsys =
                &rscfl_data->subsyses[rhdl_acct->acct_subsys[subsys_no]];
            // We are iterating over an array of pointers. These pointers will
            // be NULL if the subsystem has not been touched. We therefore just
            // continue.

            if (acct_subsys == NULL) {
              continue;
            }

            acct_subsys->cpu.cycles += rhdl_subsys->cpu.cycles;
            acct_subsys->cpu.branch_mispredictions +=
                rhdl_subsys->cpu.branch_mispredictions;
            acct_subsys->cpu.wall_clock_time +=
                rhdl_subsys->cpu.wall_clock_time;
            acct_subsys->mem.alloc += rhdl_subsys->mem.alloc;
            acct_subsys->mem.freed += rhdl_subsys->mem.freed;
          }

          // We have added the costs of rhdl_acct to acct, so can now reuse
          // rhdl_acct.
          rhdl_acct->in_use = 0;

          // We have found a matching syscall_id, so we know it won't appear
          // again in the rest of the array, as syscall_id's are unique.
          return 0;
        } else {
          rhdl_acct++;
          i++;
        }
      } else {
        rhdl_acct++;
        i++;
      }
    }
  } else {
    printf("rhdl_acct is null!");
  }
  return -1;
}

struct subsys_accounting *get_subsys_accounting(rscfl_handle rhdl,
                                                struct accounting *acct,
                                                rscfl_subsys subsys_no)
{
  if (acct->acct_subsys[subsys_no] == -1) {
    return NULL;
  }
  rscfl_shared_mem_layout_t *rscfl_data =
      (rscfl_shared_mem_layout_t *)rhdl->buf;

  return &rscfl_data->subsyses[acct->acct_subsys[subsys_no]];
}
