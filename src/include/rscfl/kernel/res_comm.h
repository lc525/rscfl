#ifndef _RES_COMM_H
#define _RES_COMM_H

#include <linux/skbuff.h>

static void res_nl_recv_msg(struct sk_buff *skb);

int _netlink_setup(void);

int _netlink_teardown(void);

#endif
