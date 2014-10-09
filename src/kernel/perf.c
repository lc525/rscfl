#include "rscfl/costs.h"
#include "rscfl/kernel/perf.h"

#include "linux/nmi.h"
#include "linux/perf_event.h"

static struct perf_event *active_counter;

static void rscfl_perf_overflow_handler(struct perf_event *event,
					struct perf_sample_data *data,
					struct pt_regs *regs)
{
  printk(KERN_ERR "rscfl perf event overflow.\n");
}

int rscfl_perf_get_current_vals(struct accounting *acct)
{
  u64 enabled;
  u64 running;
  struct perf_output_handle;
  u64 val;

  BUG_ON(active_counter == NULL);
  val = perf_event_read_value(active_counter, &enabled, &running);
  BUG_ON(!enabled);
  BUG_ON(!running);
  acct->cpu.cycles = val;
  return 0;
}

int rscfl_perf_init(void) {
  struct perf_event *pevent;
  struct perf_event_attr attr = {0};
  int cpus = 0;
  struct task_struct task;
  perf_overflow_handler_t overflow_handler;

  attr.type = PERF_TYPE_SOFTWARE;
  attr.size = sizeof(struct perf_event_attr);

  attr.config = PERF_COUNT_SW_CPU_CLOCK;

  attr.pinned = 1;

  attr.sample_period = 0;

  pevent = perf_event_create_kernel_counter(&attr, cpus, NULL,
					    rscfl_perf_overflow_handler, NULL);

  if (IS_ERR(pevent)) {
    printk(KERN_ERR "rscfl: error creating perf event kernel counter %ld.\n",
	   PTR_ERR(pevent));
    return PTR_ERR(pevent);
  }

  if (pevent->state != PERF_EVENT_STATE_ACTIVE) {
    printk(KERN_ERR "rscfl: perf event active.\n");
    return -EBUSY;
  }

  active_counter = pevent;

  printk("Counter active.\n");

  return 0;

}
