#include "rscfl/kernel/measurement.h"

#include "linux/kernel.h"
#include "linux/mm.h"
#include "linux/nmi.h"
#include "linux/time.h"
#include "linux/workqueue.h"

#include "rscfl/kernel/cpu.h"
#include "rscfl/kernel/perf.h"
#include "rscfl/kernel/priv_kallsyms.h"
#include "rscfl/kernel/probes.h"
#include "rscfl/kernel/subsys.h"
#include "rscfl/kernel/xen.h"
#include "rscfl/res_common.h"


DEFINE_HASHTABLE(tokens, TOKENS_HASH_BUCKETS);

/*
 * Some extra, useful counters
 */
/*
 *static struct timespec rscfl_get_timestamp(void)
 *{
 *  struct timespec ts;
 *  getrawmonotonic(&ts);
 *  return ts;
 *}
 */

int rscfl_counters_init(void)
{
  return xen_scheduler_init();
}

void rscfl_counters_stop(void)
{
}

int rscfl_counters_update_subsys_vals(struct subsys_accounting *add_subsys,
                                      struct subsys_accounting *minus_subsys)
{
#ifdef XEN_ENABLED
  struct shared_sched_info *sched_info = (void *)(
      // Start of the shared page
      (unsigned long)*KPRIV(HYPERVISOR_shared_info) +

      // The amount of the page that Linux uses
      sizeof(struct shared_info) +

      // Linux doesn't use the last 18 bytes of the struct, so we need to offset
      // by this amount.
      0x18);
#endif

  pid_acct *current_pid_acct;

  u64 cycles = rscfl_get_cycles();
  //struct timespec time = rscfl_get_timestamp();
  int subsys_err;
  syscall_interest_t *interest;
  struct rscfl_kernel_token *tbl_token;

  preempt_disable();
  current_pid_acct = CPU_VAR(current_acct);
  preempt_enable();

  interest = &(current_pid_acct->ctrl->interest);

  // Update the WALL CLOCK TIME and CYCLES
  if (add_subsys != NULL) {
    add_subsys->subsys_entries++;
    add_subsys->cpu.cycles += cycles;
    //rscfl_timespec_add(&add_subsys->cpu.wall_clock_time, &time);
  }

  if (minus_subsys != NULL) {
    minus_subsys->subsys_exits++;
    minus_subsys->cpu.cycles -= cycles;
    //rscfl_timespec_diff_comp(&minus_subsys->cpu.wall_clock_time, &time);
  }

#ifdef XEN_ENABLED
  // HYPERVISOR
  if (*KPRIV(HYPERVISOR_shared_info) != KPRIV(xen_dummy_shared_info) && !disable_xen) {
    // We are running in a Xen VM.
    uint64_t hypervisor_timestamp;
    int hd = sched_info->sched_hd;
    int tl = 0;
    sched_event_t *event;
    sched_event_t *event_page;

    if (add_subsys == NULL) {
      subsys_err = get_subsys(USERSPACE_XEN, &add_subsys);
      if (subsys_err != 0) {
        return subsys_err;
      }
    }

    // Update tail if we have a token.
    hash_for_each_possible(tokens, tbl_token, link, interest->token)
    {
      if (interest->token != tbl_token->id) {
        continue;
      }
      tl = tbl_token->val;
      tbl_token->val = hd;
    }

    for (; hd != tl; tl = (tl + 1) % CURRENT_XEN_NUM_EVENTS) {
      event_page = (sched_event_t *)rscfl_pages[tl / XEN_EVENTS_PER_PAGE];
      event = &event_page[tl % XEN_EVENTS_PER_PAGE];
      if (add_subsys != NULL) {
        // Get timespec from the scheduling event->
        hypervisor_timestamp = 0;
        //memset(&time, 0, sizeof(struct timespec));
        //rscfl_timespec_add_ns(&time, hypervisor_timestamp);

        // Check the number of credits for the VCPU, and update min/max as
        // required.
        add_subsys->sched.xen_credits_min =
            min(add_subsys->sched.xen_credits_min, (int)event->credit);
        add_subsys->sched.xen_credits_max =
            max(add_subsys->sched.xen_credits_max, (int)event->credit);

        if (event->sched_in) {
          // Update count of scheduling events.
          add_subsys->sched.xen_schedules++;

          //rscfl_timespec_add(&add_subsys->sched.xen_sched_wct, &time);
          add_subsys->sched.xen_sched_cycles += event->cycles;
          add_subsys->sched.xen_evtchn_pending_size += no_evtchn_events;

        } else if (event->sched_out) {
          // Update count of scheduling events.
          add_subsys->sched.xen_schedules++;

          //rscfl_timespec_diff_comp(&time, &add_subsys->sched.xen_sched_wct);
          //add_subsys->sched.xen_sched_wct = time;
          add_subsys->sched.xen_sched_cycles -= event->cycles;
          add_subsys->sched.xen_evtchn_pending_size -= no_evtchn_events;
        }

        if (event->is_block) {
          add_subsys->sched.xen_blocks++;
        }
        if (event->is_yield) {
          add_subsys->sched.xen_yields++;
        }
      }
    }
  }
#endif

  // Here we'd snapshot the Perf counters, but since they're unused at the
  // moment, we simply return.
  return 0;
}
