#ifndef _RSCFL_NETLINK_H_
#define _RSCFL_NETLINK_H_

#include <linux/skbuff.h>

static void res_nl_recv_msg(struct sk_buff *skb);

int _netlink_setup(void);

int _netlink_teardown(void);

#endif
