#include "rscfl/kernel/measurement.h"

#include "linux/nmi.h"
#include "linux/time.h"

#include "rscfl/kernel/cpu.h"
#include "rscfl/kernel/perf.h"
#include "rscfl/kernel/priv_kallsyms.h"
#include "rscfl/res_common.h"

struct sched_event
{
  uint64_t timestamp;
  uint64_t cycles;
  _Bool is_yield;
  _Bool sched_in;
};
typedef struct sched_event sched_event_t;

struct shared_sched_info
{
  uint8_t sched_tl;
  uint8_t sched_hd;
  sched_event_t sched[16];
};

/*
 * Some extra, useful counters
 */
static __inline__ ru64 rscfl_get_cycles(void)
{
  unsigned int hi, lo;
  __asm__ volatile("rdtsc" : "=a"(lo), "=d"(hi));
  return ((ru64)hi << 32) | lo;
}

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
    rscfl_timespec_diff(&minus_subsys->cpu.wall_clock_time, &time);
  }

  // Check and see if any scheduling happened underneath us. If there was,
  // update.
  // LOCAL
  if (add_subsys != NULL) {
    curr_pid = CPU_VAR(current_acct);
    acct = curr_pid->probe_data->syscall_acct;
    if (acct != NULL) {
      if (acct->wct_out_temp.tv_sec != 0 || acct->wct_out_temp.tv_nsec != 0) {
        rscfl_timespec_add(&add_subsys->sched.wct_out_local,
                           &acct->wct_out_temp);
      }
    }
  }

  // HYPERVISOR
  if (*KPRIV(HYPERVISOR_shared_info) != KPRIV(xen_dummy_shared_info)) {
    // We are running in a Xen VM.

    int hd = sched_info->sched_hd;
    int tl = sched_info->sched_tl;
    sched_info->sched_tl = hd;

    for (; hd > tl; tl = (tl + 1) % 16) {
      if (add_subsys != NULL) {
        add_subsys->sched.hypervisor_schedules++;
        hypervisor_timestamp = sched_info->sched[tl].timestamp;
        memset(&time, 0, sizeof(struct timespec));
        rscfl_timespec_add_ns(&time, hypervisor_timestamp);
        if (sched_info->sched[tl].sched_in) {
          rscfl_timespec_add(&add_subsys->sched.wct_out_hyp, &time);
          add_subsys->sched.hypervisor_cycles += sched_info->sched[tl].cycles;
        } else {
          // We're scheduling out, so we want to subtract the current cycle
          // count.
          rscfl_timespec_minus(&add_subsys->sched.wct_out_hyp, &time);
          add_subsys->sched.hypervisor_cycles -= sched_info->sched[tl].cycles;
        }
      }
    }
  }

  // Here we'd snapshot the Perf counters, but since they're unused at the
  // moment, we simply return.
  return 0;
}
