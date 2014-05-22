#include "res_user/res_api.h"

#define BIT(x) 1U << x

#include <linux/netlink.h>
#include <stddef.h>
#include <stdio.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <costs.h>
#include <string.h>

#define NETLINK_USER 31
#define MAX_PAYLOAD 1024 /* maximum payload size*/

#define syscall_name "any_syscall"

struct sockaddr_nl src_addr, dest_addr;
struct nlmsghdr *nlh = NULL;
struct iovec iov;
int sock_fd;
struct msghdr msg;

int acct_next(void)
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

  /* Read message from kernel */
  recvmsg(sock_fd, &msg, 0);
  close(sock_fd);
  return 0;
}

int read_acct(void)
{
	int relay_f;
	char buff[sizeof(struct accounting)];
	struct accounting *acct;

	relay_f = open("/mnt/resourceful0", O_RDONLY);
	if (relay_f == -1) {
		printf("Problem opening fd to relay file.\n");
		return 1;
	}

	read(relay_f, buff, sizeof(struct accounting));
	acct = (struct accounting *) buff;

	printf("Got some data.\n");
	printf("CPU Cycles %llu\n",acct->cpu.cycles);

	close(relay_f);

}
