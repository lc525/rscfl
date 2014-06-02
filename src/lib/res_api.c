#include "res_user/res_api.h"

#include <config.h>
#include <linux/netlink.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>  // malloc builtin; avoids debug compilation warning
#include <sys/socket.h>
#include <fcntl.h>
#include <costs.h>
#include <string.h>
#include <sys/mman.h>
#include <errno.h>
#include <unistd.h>

#define MAX_PAYLOAD 1024 /* maximum payload size*/

#define syscall_name "any_syscall"

static struct sockaddr_nl src_addr, dest_addr;
static struct nlmsghdr *nlh = NULL;
static struct iovec iov;
static int sock_fd;
static struct msghdr msg;

rscfl_handle rscfl_init()
{
  struct stat sb;
  int relay_fd;

  // Open the relay file
  relay_fd = open("/sys/kernel/debug/resourceful0", O_RDONLY);
  if (relay_fd == -1) {
    return NULL;
  }

  // mmap a chunk of data the size of all of the sub-buffers (def in config.h)
  char *relay_f_data = mmap(0, SUBBUF_SIZE * N_SUBBUFS, PROT_READ, MAP_SHARED,
                       relay_fd, 0);
  if (relay_f_data == MAP_FAILED) {
    return NULL;
  }

  // Return the fd to the system
  if (close(relay_fd) == -1) {
    return NULL;
  }

  return 0;
}

int rscfl_acct_next(void)
{
  sock_fd = socket(PF_NETLINK, SOCK_RAW, NETLINK_USER);
  if (sock_fd < 0) {
    return -1;
  }

  memset(&src_addr, 0, sizeof(src_addr));
  src_addr.nl_family = AF_NETLINK;
  src_addr.nl_pid = getpid(); /* self pid */

  bind(sock_fd, (struct sockaddr *)&src_addr, sizeof(src_addr));

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

  strcpy(NLMSG_DATA(nlh), syscall_name);

  iov.iov_base = (void *)nlh;
  iov.iov_len = nlh->nlmsg_len;
  msg.msg_name = (void *)&dest_addr;
  msg.msg_namelen = sizeof(dest_addr);
  msg.msg_iov = &iov;
  msg.msg_iovlen = 1;

  sendmsg(sock_fd, &msg, 0);

  close(sock_fd);
  return 0;
}

int rscfl_read_acct(rscfl_handle relay_f_data, struct accounting *acct)
{
  memcpy(acct, relay_f_data, sizeof(struct accounting));
  return 0;
}

