#include "config.h"
#include "costs.h"
#include "res_kernel/res_comm.h"
#include "res_kernel/stap_shim.h"

#include <linux/module.h>
#include <linux/netlink.h>
#include <net/sock.h>

struct sock *nl_sk = NULL;

static void res_nl_recv_msg(struct sk_buff *skb)
{
  struct nlmsghdr *nlh;
  pid_t pid;
  rscfl_syscall_id_t *lst_syscall;

  nlh = (struct nlmsghdr *)skb->data;
  pid = nlh->nlmsg_pid; /*pid of sending process */
  lst_syscall = (rscfl_syscall_id_t *)NLMSG_DATA(nlh);
  acct_next(pid, lst_syscall->id);
}

int _netlink_setup(void)
{
  struct netlink_kernel_cfg cfg = {
    .groups = 0,
    .flags =0,
    .input = res_nl_recv_msg,
    .cb_mutex = NULL,
    .bind = NULL,
    .compare = NULL
  };
  nl_sk = netlink_kernel_create(&init_net, NETLINK_USER, &cfg);
  if (!nl_sk) {
    printk(KERN_ERR "rscfl: Error creating socket.\n");
    return -10;
  }

  return 0;
}

int _netlink_teardown()
{
  if (!nl_sk) {
    return -1;
  }
  netlink_kernel_release(nl_sk);
  return 0;
}
