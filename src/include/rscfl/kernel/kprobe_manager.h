#ifndef _RSCFL_KPROBE_MANAGER_H_
#define _RSCFL_KPROBE_MANAGER_H_

#include <linux/kprobes.h>

struct rscfl_probe_list_n
{
  struct kretprobe *probe;
  struct rscfl_probe_list_n *next;
};
typedef struct rscfl_probe_list_n rscfl_probe_list_n;

int rscfl_default_pre_handler(struct kretprobe_instance *probe,
                              struct pt_regs *regs);
int rscfl_default_rtn_handler(struct kretprobe_instance *probe,
                              struct pt_regs *regs);

void rscfl_unregister_kprobes(void);

// Create and init multiple kprobes
int rscfl_init_rtn_kprobes(kprobe_opcode_t **subsys_addrs[], int num,
                           kretprobe_handler_t kp_pre_handler[],
                           kretprobe_handler_t kp_rtn_handler[]);

#endif

