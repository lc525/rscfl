#include "rscfl/kernel/measurement.h"

#include "linux/kernel.h"
#include "linux/nmi.h"
#include "linux/time.h"

#include "rscfl/kernel/cpu.h"
#include "rscfl/kernel/perf.h"
#include "rscfl/kernel/priv_kallsyms.h"
#include "rscfl/res_common.h"

#define SCHED_EVENT_BUF_SIZE 16

ru64 no_evtchn_events = 0;

struct sched_event
{
  uint64_t timestamp;
  uint64_t cycles;
  int credit;

  _Bool is_yield;

  _Bool is_block;
  _Bool is_unblock;

  _Bool sched_in;
  _Bool sched_out;
};
typedef struct sched_event sched_event_t;

struct shared_sched_info
{
  uint8_t sched_tl;
  uint8_t sched_hd;
  sched_event_t sched[SCHED_EVENT_BUF_SIZE];
};

/*
 * Some extra, useful counters
 */
static struct timespec rscfl_get_timestamp(void)
{
  struct timespec ts;
  getrawmonotonic(&ts);
  return ts;
}

/*
 * This is where we'd init the Perf counters if we were using them.
 */
int rscfl_counters_init(void)
{
  return 0;
}

void rscfl_counters_stop(void)
{
}

int rscfl_counters_update_subsys_vals(struct subsys_accounting *add_subsys,
                                      struct subsys_accounting *minus_subsys)
{
  struct shared_sched_info *sched_info = (void *)(
      // Start of the shared page
      (unsigned long)*KPRIV(HYPERVISOR_shared_info) +

      // The amount of the page that Linux uses
      sizeof(struct shared_info) +

      // Linux doesn't use the last 18 bytes of the struct, so we need to offset
      // by this amount.
      0x18);

  pid_acct *curr_pid;
  struct accounting *acct;

  uint64_t hypervisor_cycles, hypervisor_timestamp;

  u64 cycles = rscfl_get_cycles();
  struct timespec time = rscfl_get_timestamp();

  // Update the WALL CLOCK TIME and CYCLES
  if (add_subsys != NULL) {
    add_subsys->subsys_entries++;
    add_subsys->cpu.cycles += cycles;
    rscfl_timespec_add(&add_subsys->cpu.wall_clock_time, &time);
  }

  if (minus_subsys != NULL) {
    minus_subsys->subsys_exits++;
    minus_subsys->cpu.cycles -= cycles;
    rscfl_timespec_diff_comp(&minus_subsys->cpu.wall_clock_time, &time);
  }

  // HYPERVISOR
  if (*KPRIV(HYPERVISOR_shared_info) != KPRIV(xen_dummy_shared_info)) {
    // We are running in a Xen VM.

    int hd = sched_info->sched_hd;
    int tl = sched_info->sched_tl;
    int evtchn_size = 0;
    sched_info->sched_tl = hd;

    for (; hd != tl; tl = (tl + 1) % SCHED_EVENT_BUF_SIZE) {
      if (add_subsys != NULL) {
        // Get timespec from the scheduling event.
        hypervisor_timestamp = sched_info->sched[tl].timestamp;
        memset(&time, 0, sizeof(struct timespec));
        rscfl_timespec_add_ns(&time, hypervisor_timestamp);

        // Check the number of credits for the VCPU, and update min/max as
        // required.
        add_subsys->sched.xen_credits_min = min(
            add_subsys->sched.xen_credits_min, sched_info->sched[tl].credit);
        add_subsys->sched.xen_credits_max = max(
            add_subsys->sched.xen_credits_max, sched_info->sched[tl].credit);

        if (sched_info->sched[tl].sched_in) {
          // Update count of scheduling events.
          add_subsys->sched.xen_schedules++;

          rscfl_timespec_add(&add_subsys->sched.xen_sched_wct, &time);
          add_subsys->sched.xen_sched_cycles += sched_info->sched[tl].cycles;
          add_subsys->sched.xen_evtchn_pending_size += no_evtchn_events;

        } else if (sched_info->sched[tl].sched_out) {
          // Update count of scheduling events.
          add_subsys->sched.xen_schedules++;

          rscfl_timespec_diff_comp(&time, &add_subsys->sched.xen_sched_wct);
          add_subsys->sched.xen_sched_wct = time;
          add_subsys->sched.xen_sched_cycles -= sched_info->sched[tl].cycles;
          add_subsys->sched.xen_evtchn_pending_size -= no_evtchn_events;
        }

        if (sched_info->sched[tl].is_block) {
          add_subsys->sched.xen_blocks++;
        }
        if (sched_info->sched[tl].is_yield) {
          add_subsys->sched.xen_yields++;
        }
      }
    }
  }

  // Here we'd snapshot the Perf counters, but since they're unused at the
  // moment, we simply return.
  return 0;
}