#include "config.h"
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
  struct sk_buff *skb_out;
  int msg_size;
  char *msg = "Hello from resourceful";
  int res;

  msg_size = strlen(msg);
  nlh = (struct nlmsghdr *)skb->data;
  pid = nlh->nlmsg_pid; /*pid of sending process */
  acct_next(pid, -1);

  skb_out = nlmsg_new(msg_size, 0);

  if (!skb_out)
  {

    printk(KERN_ERR "rscfl: Failed to allocate new skb\n");
    return;

  }
  nlh = nlmsg_put(skb_out, 0, 0, NLMSG_DONE, msg_size, 0);
  NETLINK_CB(skb_out).dst_group = 0; /* not in mcast group */
  strncpy(nlmsg_data(nlh), msg, msg_size);

  res = nlmsg_unicast(nl_sk, skb_out, pid);

  if (res < 0)
    printk(KERN_INFO "rscfl: Error while sending bak to user\n");

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
