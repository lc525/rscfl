#include "rscfl/kernel/measurement.h"

#include "asm/pgtable_types.h"
#include "asm/xen/hypercall.h"
#include "linux/gfp.h"
#include "linux/kernel.h"
#include "linux/mm.h"
#include "linux/nmi.h"
#include "linux/time.h"
#include "linux/workqueue.h"
#include "uapi/xen/privcmd.h"
#include "xen/balloon.h"
#include "xen/interface/memory.h"
#include "asm/xen/page.h"
#include "xen/xen.h"

#include "rscfl/kernel/cpu.h"
#include "rscfl/kernel/perf.h"
#include "rscfl/kernel/priv_kallsyms.h"
#include "rscfl/kernel/probes.h"
#include "rscfl/kernel/subsys.h"
#include "rscfl/res_common.h"

#define NUM_XEN_PAGES 10
#define XEN_EVENTS_PER_PAGE 50
#define XEN_NUM_EVENTS (RSCFL_PAGES * RSCFL_EVENTS_PER_PAGE)

// TODO(oc243): We currently only use 256 events rather than all of the
// available ones, due to Xen's setup.
#define CURRENT_XEN_NUM_EVENTS 256

ru64 no_evtchn_events = 0;
short disable_xen = NOT(XEN_ENABLED);

struct sched_event
{
  uint64_t cycles;
  uint8_t credit;

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
  unsigned long rscfl_page_phys[NUM_XEN_PAGES];
};

static char *rscfl_pages[NUM_XEN_PAGES];

xen_pfn_t frame_list[PAGE_SIZE / sizeof(unsigned long)];

/*
 * Some extra, useful counters
 */
static struct timespec rscfl_get_timestamp(void)
{
  struct timespec ts;
  getrawmonotonic(&ts);
  return ts;
}

DEFINE_HASHTABLE(tokens, TOKENS_HASH_BUCKETS);

/*
 * Absolute black-magic craziness lives inside this function.
 *
 * Here we map the pages shared by Xen into rscfl. We do this by
 * relying on the MFNs of the shared pages living at the end of the shared_info
 * struct.
 *
 * We have to map these MFNs into our address space. To do this we allocate
 * pages, then make hypercalls to let Xen take back the physical memory backing
 * the pages. We then remap the associated virtual addresses.
 */
static int xen_scheduler_init(void)
{
  int rc;
  int i;
  struct shared_sched_info *sched_info = (void *)(
      // Start of the shared page
      (unsigned long)*KPRIV(HYPERVISOR_shared_info) +

      // The amount of the page that Linux uses
      sizeof(struct shared_info) +

      // Linux doesn't use the last 18 bytes of the struct, so we need to offset
      // by this amount.
      0x18);

  struct xen_memory_reservation reservation = {
      .address_bits = 0, .extent_order = 0, .domid = DOMID_SELF,
  };

  struct page *pg;

  if (*KPRIV(HYPERVISOR_shared_info) == KPRIV(xen_dummy_shared_info) || disable_xen) {
    // Not running on Xen.
    return 0;
  }

  for (i = 0; i < NUM_XEN_PAGES; i++) {
    unsigned long pfn;
    // Allocate pages so we get virtual addresses that we're going to later
    // map onto the MFNs.
    pg = alloc_page(GFP_KERNEL);
    pfn = page_to_pfn(pg);

    // Use a mechanism like the balloon driver to give the physical memory
    // back to Xen.
    if (!PageHighMem(pg)) {
      struct page *scratch_page = KPRIV(get_balloon_scratch_page)();
      rc = HYPERVISOR_update_va_mapping(
          (unsigned long)__va(pfn << PAGE_SHIFT),
          pfn_pte(page_to_pfn(scratch_page), PAGE_KERNEL_RO), 0);
      BUG_ON(rc);
      KPRIV(put_balloon_scratch_page)();

      KPRIV(__set_phys_to_machine)(pfn, INVALID_P2M_ENTRY);
    }
    if (current == NULL) {
      printk("Current NULL\n");
    }

    frame_list[0] = pfn_to_mfn(pfn);

    KPRIV(flush_tlb_all)();

    set_xen_guest_handle(reservation.extent_start, frame_list);
    reservation.nr_extents = 1;
    rc = HYPERVISOR_memory_op(XENMEM_decrease_reservation, &reservation);

    KPRIV(flush_tlb_all)();

    if (rc) {
      printk(KERN_ERR
             "rscfl: error decrease reservation to map rscfl pages %d\n",
             rc);
      return rc;
    }

    // Now we have given the physical memory back to Xen, adjust our own PTEs
    // to point at the shared memory.
    rc = ioremap_page_range((unsigned long)page_address(pg),
                            (unsigned long)page_address(pg) + PAGE_SIZE,
                            sched_info->rscfl_page_phys[i] << PAGE_SHIFT,
                            PAGE_KERNEL);
    if (rc) {
      printk(KERN_ERR
             "rscfl: error remapping the virtual addresses to point at the "
             "rscfl pages. err%d \n",
             rc);
      return rc;
    }
    rscfl_pages[i] = page_address(pg);
  }
  return 0;
}

int rscfl_counters_init(void)
{
  return xen_scheduler_init();
}

void rscfl_counters_stop(void)
{
}

/*
 * Find the location of the hd pointer for Xen scheduling events.
 */
int xen_buffer_hd(void)
{
  struct shared_sched_info *sched_info = (void *)(
      // Start of the shared page
      (unsigned long)*KPRIV(HYPERVISOR_shared_info) +

      // The amount of the page that Linux uses
      sizeof(struct shared_info) +

      // Linux doesn't use the last 18 bytes of the struct, so we need to offset
      // by this amount.
      0x18);

  if (*KPRIV(HYPERVISOR_shared_info) != KPRIV(xen_dummy_shared_info) && !disable_xen) {
    // We are running in a Xen VM.
    return sched_info->sched_hd;
  } else {
    return 0;
  }
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

  pid_acct *current_pid_acct;

  uint64_t hypervisor_timestamp;

  u64 cycles = rscfl_get_cycles();
  struct timespec time = rscfl_get_timestamp();
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
    rscfl_timespec_add(&add_subsys->cpu.wall_clock_time, &time);
  }

  if (minus_subsys != NULL) {
    minus_subsys->subsys_exits++;
    minus_subsys->cpu.cycles -= cycles;
    rscfl_timespec_diff_comp(&minus_subsys->cpu.wall_clock_time, &time);
  }

  // HYPERVISOR
  if (*KPRIV(HYPERVISOR_shared_info) != KPRIV(xen_dummy_shared_info) && !disable_xen) {
    // We are running in a Xen VM.

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
        memset(&time, 0, sizeof(struct timespec));
        rscfl_timespec_add_ns(&time, hypervisor_timestamp);

        // Check the number of credits for the VCPU, and update min/max as
        // required.
        add_subsys->sched.xen_credits_min =
            min(add_subsys->sched.xen_credits_min, (int)event->credit);
        add_subsys->sched.xen_credits_max =
            max(add_subsys->sched.xen_credits_max, (int)event->credit);

        if (event->sched_in) {
          // Update count of scheduling events.
          add_subsys->sched.xen_schedules++;

          rscfl_timespec_add(&add_subsys->sched.xen_sched_wct, &time);
          add_subsys->sched.xen_sched_cycles += event->cycles;
          add_subsys->sched.xen_evtchn_pending_size += no_evtchn_events;

        } else if (event->sched_out) {
          // Update count of scheduling events.
          add_subsys->sched.xen_schedules++;

          rscfl_timespec_diff_comp(&time, &add_subsys->sched.xen_sched_wct);
          add_subsys->sched.xen_sched_wct = time;
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

  // Here we'd snapshot the Perf counters, but since they're unused at the
  // moment, we simply return.
  return 0;
}
