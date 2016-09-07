global {
  subsystem_whitelist: net_link_layer
}

subsystem net_link_layer {
  boundary:
    probe dev_queue_xmit {
      arg: skb
      capture: {
        name: net_buf_enq,
        val: &skb->dev->qdisc
    },
    probe qdisk_restart {
      arg: dev
      capture: {
        name: net_buf_deq,
        val: &dev->qdisc
    }
  metrics: cycles
  map_async: match(net_buf_deq, net_buf_enq)
}
