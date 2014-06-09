#include "config.h"
#include "costs.h"
#include "res_user/res_api.h"
#include "res_common.h"

#include <linux/netlink.h>
#include <linux/types.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>  // malloc builtin; avoids debug compilation warning
#include <sys/socket.h>
#include <fcntl.h>
#include <string.h>
#include <sys/mman.h>
#include <errno.h>
#include <unistd.h>

#define NO_RELAY_ACCTS (SUBBUF_SIZE * N_SUBBUFS) / sizeof(struct accounting)
#define MAX_PAYLOAD 1024 /* maximum payload size*/

static struct sockaddr_nl src_addr, dest_addr;
static struct nlmsghdr *nlh = NULL;
static struct iovec iov;
static int sock_fd;
static struct msghdr msg;

rscfl_handle rscfl_init()
{
  struct stat sb;
  int fd = open("/dev/rscfl", O_RDWR);
  rscfl_handle relay_f_data = (rscfl_handle) malloc(sizeof(*relay_f_data));
  if (!relay_f_data) {
    return NULL;
  }

  if (fd == -1) {
    goto error;
  }


  // mmap a chunk of data the size of all of the sub-buffers (def in config.h)
  relay_f_data->buf = mmap(NULL, 4096, PROT_READ | PROT_WRITE, MAP_PRIVATE, fd, 0);
  if (relay_f_data == MAP_FAILED) {
    goto error;
  }

  // Return the fd to the system
  if (close(fd) == -1) {
    goto error;
  }

  return relay_f_data;

error:
  if (relay_f_data) {
    if (relay_f_data->buf) {
      munmap(relay_f_data->buf, SUBBUF_SIZE);
    }
    free(relay_f_data);
  }
  return NULL;
}

int rscfl_acct_next(rscfl_handle relay_f_data)
{
  int rc;
  sock_fd = socket(PF_NETLINK, SOCK_RAW, NETLINK_USER);
  if (sock_fd < 0) {
    return -1;
  }

  memset(&src_addr, 0, sizeof(src_addr));
  src_addr.nl_family = AF_NETLINK;
  src_addr.nl_pid = getpid(); /* self pid */

  if (rc = bind(sock_fd, (struct sockaddr *)&src_addr, sizeof(src_addr))) {
    return rc;
  }

  memset(&dest_addr, 0, sizeof(dest_addr));
  memset(&dest_addr, 0, sizeof(dest_addr));
  dest_addr.nl_family = AF_NETLINK;
  dest_addr.nl_pid = 0; /* For Linux Kernel */
  dest_addr.nl_groups = 0; /* unicast */

  nlh = (struct nlmsghdr *)malloc(NLMSG_SPACE(MAX_PAYLOAD));
  memset(nlh, 0, NLMSG_SPACE(MAX_PAYLOAD));
  nlh->nlmsg_len = NLMSG_SPACE(MAX_PAYLOAD);
  nlh->nlmsg_pid = getpid();
  nlh->nlmsg_flags = 0;

  memcpy(NLMSG_DATA(nlh), &relay_f_data->lst_syscall,
         sizeof(rscfl_syscall_id_t));

  iov.iov_base = (void *)nlh;
  iov.iov_len = nlh->nlmsg_len;
  msg.msg_name = (void *)&dest_addr;
  msg.msg_namelen = sizeof(dest_addr);
  msg.msg_iov = &iov;
  msg.msg_iovlen = 1;

  sendmsg(sock_fd, &msg, 0);

  close(sock_fd);

  relay_f_data->lst_syscall.id++;

  return 0;
}

int rscfl_read_acct(rscfl_handle relay_f_data, struct accounting *acct)
{
  struct accounting *relay_acct = (struct accounting *) relay_f_data->buf;
  while (relay_acct - (struct accounting *) relay_f_data->buf <
	 NO_RELAY_ACCTS) {
    if (relay_acct->syscall_id.id == (relay_f_data->lst_syscall.id - 1)) {
      memcpy(acct, relay_acct, sizeof(struct accounting));
      relay_acct->in_use = 0;
      return 0;
    }
    else {
      relay_acct++;
    }
  }
  return -1;
}

