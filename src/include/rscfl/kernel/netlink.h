#ifndef _RSCFL_NETLINK_H
#define _RSCFL_NETLINK_H

#include <linux/skbuff.h>

static void res_nl_recv_msg(struct sk_buff *skb);

int _netlink_setup(void);

int _netlink_teardown(void);

#endif
