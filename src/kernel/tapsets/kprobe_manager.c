#include <linux/kernel.h>
#include <linux/slab.h>

#include "kprobe_manager.h"

static rscfl_probe_list_n *probe_list;

int rscfl_default_pre_handler(struct kretprobe_instance *probe,
                              struct pt_regs *regs)
{
  return 0;
}

int rscfl_default_rtn_handler(struct kretprobe_instance *probe,
                              struct pt_regs *regs)
{
  return 0;
}

struct kretprobe *rscfl_create_probe(kprobe_opcode_t *address,
                                     kretprobe_handler_t kp_pre_handler,
                                     kretprobe_handler_t kp_rtn_handler)
{
  struct kretprobe *probe = kzalloc(sizeof(struct kretprobe), GFP_KERNEL);
  if (!probe) {
    return NULL;
  } else {
    probe->handler = kp_rtn_handler;
    probe->entry_handler = kp_pre_handler;
    probe->kp.addr = address;
    probe->maxactive = 0;
    return probe;
  }
}

void rscfl_unregister_kprobes(void)
{
  rscfl_probe_list_n *probes_head = probe_list;
  while (probes_head != NULL) {
    rscfl_probe_list_n *next = probes_head->next;
    unregister_kretprobe(probes_head->probe);
    kfree(probes_head->probe);
    kfree(probes_head);
    probes_head = next;
  }
}

int rscfl_init_rtn_kprobes(kprobe_opcode_t **subsys_addrs[], int num,
                           kretprobe_handler_t kp_pre_handler,
                           kretprobe_handler_t kp_rtn_handler)
{
  rscfl_probe_list_n *probe_head = NULL;
  rscfl_probe_list_n *probe_tail = NULL;

  int success_count = 0;
  int fail_count = 0;

  /* Subsys addr layout:
   * |SUBSYS|,|SUBSYS|,|SUBSYS|
   * |SUBSYS| -> 0x..., 0x..., 0x..., 0x...
   */
  int i;
  for (i = 0; i < num; i++) {
    kprobe_opcode_t **sub_addr = subsys_addrs[i];

    while (*sub_addr != 0) {
      struct kretprobe *probe =
          rscfl_create_probe(*sub_addr, kp_pre_handler, kp_rtn_handler);
      if (probe != NULL) {
        int rtn;
        if ((rtn = register_kretprobe(probe)) < 0) {
          printk("Error setting kprobe on address:%x error:%d\n", *sub_addr,
                 rtn);
          fail_count++;
          kfree(probe);
        } else {
          rscfl_probe_list_n *curr_probe = (rscfl_probe_list_n *)kzalloc(
              sizeof(rscfl_probe_list_n), GFP_KERNEL);
          if (!curr_probe) {
            printk("Couldn't create new list entry\n");
            kfree(probe);
          } else {
            curr_probe->probe = probe;
            if (probe_head == NULL) {
              probe_head = curr_probe;
              probe_tail = curr_probe;
            } else {
              probe_tail->next = curr_probe;
              probe_tail = curr_probe;
            }
            success_count++;
          }
        }
      } else {
        printk("Error creating probe for address:%x\n", *sub_addr);
      }
      sub_addr++;
    }
  }
  printk("Done creating and registering probes\n");
  probe_list = probe_head;
  return success_count;
}

