#include "linux/nmi.h"
#include "linux/perf_event.h"

#include "rscfl/costs.h"
#include "rscfl/kernel/cpu.h"
#include "rscfl/kernel/perf.h"
#include "rscfl/kernel/stap_shim.h"

#define NUM_SW_EVENTS sizeof(sw_events) / sizeof(sw_events[0])

static void rscfl_perf_overflow_handler(struct perf_event *event,
                                        struct perf_sample_data *data,
                                        struct pt_regs *regs)
{
  printk(KERN_ERR "rscfl perf event overflow.\n");
}

static const __u64 sw_events[] = {
    PERF_COUNT_SW_CPU_CLOCK,        PERF_COUNT_SW_PAGE_FAULTS,
    PERF_COUNT_SW_TASK_CLOCK,       PERF_COUNT_SW_ALIGNMENT_FAULTS,
    PERF_COUNT_SW_EMULATION_FAULTS,
};

static struct perf_event *event_counters[NUM_SW_EVENTS];

/*
 * Use perf to read the current resources, and store them in acct_subsys.
 */
static int rscfl_perf_get_current_vals(struct subsys_accounting *acct_subsys,
                                       _Bool add)
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
    if (add) {
      switch (sw_events[i]) {
      case PERF_COUNT_SW_CPU_CLOCK:
        acct_subsys->cpu.cycles += val;
        break;
      case PERF_COUNT_SW_PAGE_FAULTS:
        acct_subsys->mem.page_faults += val;
        break;
      case PERF_COUNT_SW_ALIGNMENT_FAULTS:
        acct_subsys->mem.align_faults += val;
        break;
      }
    } else {
      switch (sw_events[i]) {
      case PERF_COUNT_SW_CPU_CLOCK:
        acct_subsys->cpu.cycles -= val;
        break;
      case PERF_COUNT_SW_PAGE_FAULTS:
        acct_subsys->mem.page_faults -= val;
        break;
      case PERF_COUNT_SW_ALIGNMENT_FAULTS:
        acct_subsys->mem.align_faults -= val;
        break;
      }
    }
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

/*
 * Find the subsys_accounting for the current struct accounting with the
 * given subsys_id.
 *
 * Must be called with preemption disabled as we cache the current process.
 */
static struct subsys_accounting *get_subsys(rscfl_subsys subsys_id)
{
  struct accounting *acct = NULL;
  char x[] = "";

  if (_should_acct(current->pid, -1, 0, x, &acct)) {
    struct subsys_accounting *subsys_acct;
    pid_acct *current_pid_acct;
    rscfl_shared_mem_layout_t *rscfl_mem;
    int subsys_offset;
    current_pid_acct = CPU_VAR(current_acct);
    rscfl_mem = current_pid_acct->shared_buf;
    subsys_offset = acct->acct_subsys[subsys_id];
    if (subsys_offset == -1) {
      debugk("looking for a subsys\n");
      // Need to find space in the page where we can store the subsystem.
      subsys_acct = rscfl_mem->subsyses;

      // Walk through the subsyses, being careful not to wonder of the end of
      // our
      // memory.
      while (subsys_acct - rscfl_mem->subsyses <= ACCT_SUBSYS_NUM) {
        if (!subsys_acct->in_use) {
          // acct_subsys is an index that describes the offset from the start
          // of
          // subsyses as measured by number of struct subsys_accountings.
          // Recall that this is done as we need consistent indexing between
          // userspace and kernel space.
          subsys_offset = subsys_acct - rscfl_mem->subsyses;
          acct->acct_subsys[subsys_id] = subsys_offset;
          subsys_acct->in_use = true;
          break;
        } else {
          subsys_acct++;
        }
      }
      if (subsys_offset == -1) {
        // We haven't found anywhere in the shared page where we can store
        // this
        // subsystem.
        debugk("error asdfhsd");
      }
      // Now need to initialise the subsystem's resources to be 0.
      subsys_acct = &rscfl_mem->subsyses[subsys_offset];
      memset(subsys_acct, 0, sizeof(struct subsys_accounting));

    } else {
      subsys_acct = &rscfl_mem->subsyses[subsys_offset];
    }
    preempt_enable();
    return subsys_acct;
  }
  return NULL;
}

void rscfl_subsystem_entry(rscfl_subsys subsys_id)
{
  pid_acct *current_pid_acct;
  struct subsys_accounting *subsys_acct;
  preempt_disable();
  subsys_acct = get_subsys(subsys_id);
  current_pid_acct = CPU_VAR(current_acct);
  if (subsys_acct != NULL) {
    current_pid_acct->probe_data->nest_level++;
    rscfl_perf_get_current_vals(subsys_acct, 0);
  }
  preempt_enable();
}

void rscfl_subsystem_exit(rscfl_subsys subsys_id)
{
  pid_acct *current_pid_acct;
  preempt_disable();
  current_pid_acct = CPU_VAR(current_acct);
  if ((current_pid_acct != NULL) &&
      (current_pid_acct->probe_data->syscall_acct)) {
    // This syscall is being accounted for.
    struct subsys_accounting *subsys_acct = get_subsys(subsys_id);
    rscfl_perf_get_current_vals(subsys_acct, 1);
    current_pid_acct->probe_data->nest_level--;
    if (!current_pid_acct->probe_data->nest_level) {
      // We have backed out of all nesting.
      _clear_acct_next(current->pid, -1);
    }
  }
  preempt_enable();
}
