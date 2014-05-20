#include <linux/module.h>
#include <net/sock.h>
#include <linux/netlink.h>
#include <linux/skbuff.h>

#define NETLINK_USER 31

extern int acct_next(void);

struct sock *nl_sk = NULL;

static void res_nl_recv_msg(struct sk_buff *skb)
{
	struct nlmsghdr *nlh;
	int pid;
	struct sk_buff *skb_out;
	int msg_size;
	char *msg = "Hello from resourceful";
	int res;

	printk(KERN_INFO "Entering: %s\n", __FUNCTION__);

	msg_size = strlen(msg);

	nlh = (struct nlmsghdr *)skb->data;
	printk(KERN_INFO "Netlink received msg payload: %s\n", (char *)nlmsg_data(nlh));
	pid = nlh->nlmsg_pid; /*pid of sending process */

	printk("Accounting next call for pid %d\n", pid);
	acct_next();

	skb_out = nlmsg_new(msg_size, 0);

	if (!skb_out)
	{

		printk(KERN_ERR "Failed to allocate new skb\n");
		return;

	}
	nlh = nlmsg_put(skb_out, 0, 0, NLMSG_DONE, msg_size, 0);
	NETLINK_CB(skb_out).dst_group = 0; /* not in mcast group */
	strncpy(nlmsg_data(nlh), msg, msg_size);

	res = nlmsg_unicast(nl_sk, skb_out, pid);

	if (res < 0)
		printk(KERN_INFO "Error while sending bak to user\n");

}

static int netlink_setup(void)
{
	struct netlink_kernel_cfg cfg = {
		.groups = 0,
		.flags =0,
		.input = res_nl_recv_msg,
		.cb_mutex = NULL,
		.bind = NULL,
		.compare = NULL
	};
	printk("Entering: %s\n", __FUNCTION__);
	nl_sk = netlink_kernel_create(&init_net, NETLINK_USER, &cfg);
	if (!nl_sk)
	{

	        printk(KERN_ALERT "Error creating socket.\n");
	        return -10;

	}

	return 0;
}

static int __init res_init(void)
{
	int rc = 0;
	if (rc = netlink_setup()) {
		goto error;
	}
	
	return 0;
error:
	return rc;
	
}

static void __exit res_exit(void)
{

	printk(KERN_INFO "exiting res module\n");
	netlink_kernel_release(nl_sk);
}


module_init(res_init);
module_exit(res_exit);
