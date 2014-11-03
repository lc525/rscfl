#ifndef KPROBE_MANAGER_H
#define KPROBE_MANAGER_H

#include<linux/kprobes.h>

struct rscfl_probe_list_n {
	struct kretprobe *probe;
	struct rscfl_probe_list_n *next;
};
typedef struct rscfl_probe_list_n rscfl_probe_list_n;

int rscfl_default_pre_handler(struct kretprobe_instance *probe, struct pt_regs *regs);
int rscfl_default_rtn_handler(struct kretprobe_instance *probe, struct pt_regs *regs);

// Create a kprobe
struct kretprobe *rscfl_create_probe(kprobe_opcode_t *address,
                                  kretprobe_handler_t kp_pre_handler,
                                  kretprobe_handler_t kp_rtn_handler);

// Un-register and free multiple probes
void rscfl_unregister_kprobes(rscfl_probe_list_n *probes_head);

// Create and init multiple kprobes
rscfl_probe_list_n *rscfl_init_rtn_kprobes(kprobe_opcode_t **subsys_addrs[],
                                           int *num,
                                           kretprobe_handler_t kp_pre_handler,
                                           kretprobe_handler_t kp_rtn_handler);

#endif

