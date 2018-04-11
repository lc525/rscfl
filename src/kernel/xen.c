/**** Notice
 * xen.c: rscfl source code
 *
 * Copyright 2015-2017 The rscfl owners <lucian.carata@cl.cam.ac.uk>
 *
 * This file is part of the rscfl open-source project: github.com/lc525/rscfl;
 * Its licensing is governed by the LICENSE file at the root of the project.
 **/

#define _RSCFL_XEN_IMPL_
#include "rscfl/kernel/xen.h"

#include "asm/pgtable_types.h"
#include "asm/xen/hypercall.h"
#include "asm/xen/page.h"
#include "linux/gfp.h"
#include "uapi/xen/privcmd.h"
#include "xen/balloon.h"
#include "xen/interface/memory.h"
#include "xen/xen.h"

#include "rscfl/kernel/priv_kallsyms.h"
#include "rscfl/res_common.h"

short disable_xen = NOT(XEN_ENABLED);
ru64 no_evtchn_events = 0;
char *rscfl_pages[NUM_XEN_PAGES];

xen_pfn_t frame_list[PAGE_SIZE / sizeof(unsigned long)];

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
int xen_scheduler_init(void)
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
  printk(KERN_INFO "rscfl: Xen domain measurements enabled\n");
  debugk(RDBG_FINE, KERN_ERR "sched_info size: %lu\n", sizeof(struct shared_info));
  debugk(RDBG_FINE, KERN_ERR "shared_sched_info size: %lu\n", sizeof(struct shared_sched_info));

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
      printk("xen_scheduler_init: current task is NULL\n");
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
    // printk("rscfl_page_phys[i]: %lu\n", sched_info->rscfl_page_phys[i]);
    rc = ioremap_page_range((unsigned long)page_address(pg),
                            (unsigned long)page_address(pg) + PAGE_SIZE,
                            sched_info->rscfl_page_phys[i] << PAGE_SHIFT,
                            PAGE_KERNEL);
    if (rc) {
      printk(KERN_ERR
             "rscfl (xen): error remapping the virtual addresses to point at the"
             "rscfl pages. err%d \n",
             rc);
      return rc;
    }
    rscfl_pages[i] = page_address(pg);
  }
  //printk("xen domain sched_out: %ul\n", sched_info->sched_out);
  //sched_info->sched_out = 0;
  return 0;
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

uint64_t xen_current_sched_out(void)
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
    return sched_info->sched_out;
  } else {
    return 0;
  }

}

void xen_clear_current_sched_out(void)
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
    sched_info->sched_out = 0;
  }
}
