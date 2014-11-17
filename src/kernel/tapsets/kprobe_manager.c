#include "rscfl/kernel/kprobe_manager.h"
#include <linux/kernel.h>
#include <linux/slab.h>

#include "rscfl/kernel/cpu.h"
#include "rscfl/subsys_list.h"

static rscfl_probe_list_n *probe_list;

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
  probe_list = NULL;
}

int rscfl_init_rtn_kprobes(kprobe_opcode_t **subsys_addrs[], int num,
                           kretprobe_handler_t kp_pre_handler[],
                           kretprobe_handler_t kp_rtn_handler[])
{
  rscfl_probe_list_n *probe_head = NULL;
  rscfl_probe_list_n *probe_tail = NULL;

  int fail_count = 0;
  int success_count = 0;

  /* Subsys addr layout:
   * |SUBSYS|,|SUBSYS|,|SUBSYS|
   * |SUBSYS| -> 0x..., 0x..., 0x..., 0x...
   */
  int i;
  for (i = 0; i < num; i++) {
    kprobe_opcode_t **sub_addr = subsys_addrs[i];

    while (*sub_addr) {
      int rtn;
      /* Create a probe */
      struct kretprobe *probe = kzalloc(sizeof(struct kretprobe), GFP_KERNEL);
      if (!probe) {
        goto error_no_mem;
      }
      probe->handler = kp_rtn_handler[i];
      probe->entry_handler = kp_pre_handler[i];
      probe->kp.addr = *sub_addr;
      probe->maxactive = 0;
      probe->data_size = sizeof(kprobe_priv);

      /* Try to register it */
      if ((rtn = register_kretprobe(probe)) < 0) {
        printk(KERN_ERR
	       "Error setting kprobe on address:%p error:%d\n", *sub_addr, rtn);
        fail_count++;
        kfree(probe);
      } else {
        rscfl_probe_list_n *curr_probe = (rscfl_probe_list_n *)kzalloc(
            sizeof(rscfl_probe_list_n), GFP_KERNEL);
        if (!curr_probe) {
          kfree(probe);
          goto error_no_mem;
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
      sub_addr++;
    }
  }
  printk("Done creating and registering: %d probes\n", success_count);
  probe_list = probe_head;
  return fail_count;

error_no_mem:
  rscfl_unregister_kprobes();
  return -ENOMEM;
}

