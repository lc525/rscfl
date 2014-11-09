#include "rscfl/kernel/perf.h"

#include "linux/nmi.h"
#include "linux/perf_event.h"

#include "rscfl/costs.h"
#include "rscfl/kernel/cpu.h"
#include "rscfl/kernel/stap_shim.h"

#define NUM_SW_EVENTS sizeof(sw_events) / sizeof(sw_events[0])

static __inline__ ru64 rscfl_get_cycles(void)
{
    unsigned int hi, lo;
    __asm__ volatile("rdtsc" : "=a" (lo), "=d" (hi));
    return ((ru64)hi << 32) | lo;
}

static void rscfl_perf_overflow_handler(struct perf_event *event,
                                        struct perf_sample_data *data,
                                        struct pt_regs *regs)
{
  printk(KERN_ERR "rscfl perf event overflow.\n");
}

static const __u64 sw_events[] = {
    PERF_COUNT_SW_PAGE_FAULTS,
    PERF_COUNT_SW_TASK_CLOCK,
    PERF_COUNT_SW_ALIGNMENT_FAULTS,
};

static struct perf_event *event_counters[NUM_SW_EVENTS];

/*
 * Use perf to read the current resources, and store them in acct_subsys.
 */
int rscfl_perf_get_current_vals(struct subsys_accounting *acct_subsys,
                                _Bool add)
{
  u64 enabled;
  u64 running;
  struct perf_output_handle;
  u64 val;
  int i;

  for (i = 0; i < NUM_SW_EVENTS; i++) {
    BUG_ON(event_counters[i] == NULL);
    preempt_enable();
    val = perf_event_read_value(event_counters[i], &enabled, &running);
    preempt_disable();
    BUG_ON(!enabled);
    BUG_ON(!running);
    if (add) {
      switch (sw_events[i]) {
      case PERF_COUNT_SW_PAGE_FAULTS:
        acct_subsys->mem.page_faults += val;
        break;
      case PERF_COUNT_SW_ALIGNMENT_FAULTS:
        acct_subsys->mem.align_faults += val;
        break;
      }
    } else {
      switch (sw_events[i]) {
      case PERF_COUNT_SW_PAGE_FAULTS:
        acct_subsys->mem.page_faults -= val;
        break;
      case PERF_COUNT_SW_ALIGNMENT_FAULTS:
        acct_subsys->mem.align_faults -= val;
        break;
      }
    }
  }

  /* Get the CPU cycle count and set it */
  val = rscfl_get_cycles();
  if (add) {
    acct_subsys->cpu.cycles += val;
  } else {
    acct_subsys->cpu.cycles -= val;
  }

  return 0;
}

static int rscfl_perf_create_counter(__u64 config, struct perf_event **pevent)
{
  struct perf_event_attr attr = {0};
  int cpus = 0;
  struct task_struct task;
  perf_overflow_handler_t overflow_handler;

  attr.type = PERF_TYPE_SOFTWARE;
  attr.size = sizeof(struct perf_event_attr);

  attr.config = config;

  attr.pinned = 1;

  attr.sample_period = 0;

  *pevent = perf_event_create_kernel_counter(&attr, cpus, NULL,
                                             rscfl_perf_overflow_handler, NULL);

  if (IS_ERR(*pevent)) {
    printk(KERN_ERR "rscfl: error creating perf event kernel counter %ld.\n",
           PTR_ERR(*pevent));
    return PTR_ERR(*pevent);
  }

  if ((*pevent)->state != PERF_EVENT_STATE_ACTIVE) {
    printk(KERN_ERR "rscfl: perf event active.\n");
    return -EBUSY;
  }
  return 0;
}

int rscfl_perf_init(void)
{
  int i;
  int rc;

  for (i = 0; i < sizeof(sw_events) / sizeof(sw_events[0]); i++) {
    rc = rscfl_perf_create_counter(sw_events[i], &event_counters[i]);
    if (rc) {
      return rc;
    }
  }

  return 0;
}
