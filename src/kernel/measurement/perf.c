/**** Notice
 * perf.c: rscfl source code
 *
 * Copyright 2015-2017 The rscfl owners <lucian.carata@cl.cam.ac.uk>
 *
 * This file is part of the rscfl open-source project: github.com/lc525/rscfl;
 * Its licensing is governed by the LICENSE file at the root of the project.
 **/

#include "rscfl/kernel/perf.h"

#include "linux/perf_event.h"
#include "linux/smp.h"

#include "rscfl/costs.h"
#include "rscfl/kernel/cpu.h"

#define NUM_SW_EVENTS sizeof(sw_events) / sizeof(sw_events[0])

static const __u64 sw_events[] = {PERF_COUNT_SW_PAGE_FAULTS,
                                  PERF_COUNT_SW_ALIGNMENT_FAULTS};

static struct perf_event *event_counters[NUM_SW_EVENTS];

static void rscfl_perf_overflow_handler(struct perf_event *event,
                                        struct perf_sample_data *data,
                                        struct pt_regs *regs)
{
  u32 cpu = smp_processor_id();
  printk(KERN_ERR "rscfl perf event overflow on cpu %u.\n", cpu);
}

static int rscfl_perf_create_counter(__u64 config, struct perf_event **pevent)
{
  struct perf_event_attr attr = {0};
  /* Only register counters on the current CPU. This doesn't so much matter for
   * the SW counters, but is here to prevent future bugs when we add in HW
   * counters.
   */
  int cpus = smp_processor_id();

  attr.type = PERF_TYPE_SOFTWARE;
  attr.size = sizeof(struct perf_event_attr);

  /* Useful configuration options */
  attr.config = config;
  attr.sample_period = 0;
  attr.exclude_user = 1;

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

void rscfl_perf_stop(void)
{
  int i;
  struct perf_event *pevent;

  for (i = 0; i < sizeof(sw_events) / sizeof(sw_events[0]); i++) {
    pevent = event_counters[i];
    if (pevent) {
      perf_event_release_kernel(pevent);
    }
  }
}

int rscfl_snapshot_perf(struct subsys_accounting *add_subsys,
                        struct subsys_accounting *minus_subsys)
{
  u64 enabled;
  u64 running;
  struct perf_output_handle;
  u64 val;
  int i;

  for (i = 0; i < NUM_SW_EVENTS; i++) {
    BUG_ON(event_counters[i] == NULL);
    val = perf_event_read_value(event_counters[i], &enabled, &running);
    BUG_ON(!enabled);
    BUG_ON(!running);
    switch (sw_events[i]) {
      case PERF_COUNT_SW_PAGE_FAULTS:
        if (add_subsys != NULL) {
          add_subsys->mem.page_faults += val;
        }
        if (minus_subsys != NULL) {
          minus_subsys->mem.page_faults -= val;
        }
        break;
      case PERF_COUNT_SW_ALIGNMENT_FAULTS:
        if (add_subsys != NULL) {
          add_subsys->cpu.alignment_faults += val;
        }
        if (minus_subsys != NULL) {
          minus_subsys->cpu.alignment_faults -= val;
        }
        break;
    }
  }
  return 0;
}
