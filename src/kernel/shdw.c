#include "rscfl/kernel/shdw.h"

#include <asm/page.h>
#include <asm/pgtable_types.h>
#include <asm/xen/hypercall.h>
#include <asm/xen/page.h>
// Needs to be here as pgtable.h depends on spinlock_types.h. *sigh*
#include <linux/spinlock_types.h>
#include <asm-generic/sections.h>
#include <linux/bootmem.h>
#include <linux/cma.h>
#include <linux/dma-contiguous.h>
#include <linux/io.h>
#include <linux/slab.h>
#include <linux/stop_machine.h>
#include <linux/vmalloc.h>
#include <xen/xen.h>

#include "rscfl/res_common.h"
#include "rscfl/kernel/cpu.h"
#include "rscfl/kernel/chardev.h"
#include "rscfl/kernel/priv_kallsyms.h"

#define MAX_SHDW_KERNELS 1000
#define NUM_PTES 10
#define TEXT_PAGES ((text_size - 1) / PAGE_SIZE + 1)

unsigned int no_shdws = 0;
DEFINE_SPINLOCK(shdw_lock);

static unsigned long phys_texts[MAX_SHDW_KERNELS];

static shdw_hdl current_shdw;

struct cma
{
  unsigned long base_pfn;
  unsigned long count;
  unsigned long *bitmap;
  unsigned int order_per_bit; /* Order of pages represented by one bit */
  struct mutex lock;
};

static char *text_start;
static char *text_end;
static unsigned long text_size;

int shdw_create(shdw_hdl *shdw_no)
{
  char *shdw_mem;
  struct page *page;
  pte_t pte;

  if (!is_vm()) {
    printk(KERN_ERR "Cannot create a shadow kernel without a hypervisor\n");
    return -EINVAL;
  }

  text_start = (char *)KPRIV(_text);
  text_end = (char *)KPRIV(_etext);
  text_size = text_end - text_start;

  if (no_shdws >= MAX_SHDW_KERNELS) {
    printk(KERN_ERR "Maximum number of shadow kernels exceeded.\n");
    return -ENOMEM;
  } else {
    no_shdws++;
  }

  spin_lock(&shdw_lock);
  *shdw_no = ++no_shdws;
  spin_unlock(&shdw_lock);

  // Allocate memory for the new kernel text section. This will be contiguous in
  // physical memory.

  page = KPRIV(dma_alloc_from_contiguous)(rscfl_ctrl_device,
                                          text_size / PAGE_SIZE + 1, 0);
  if (page == NULL) {
    return -ENOMEM;
  }
  shdw_mem = __va((char *)page_to_phys(page));
  if (shdw_mem == NULL) {
    return -ENOMEM;
  }

  // Copy the current kernel text section into this new memory.
  memcpy(shdw_mem, text_start, text_size);

  // Store the physical address of the shadow memory.
  phys_texts[*shdw_no] = __pa(shdw_mem);

  debugk("Created shadow kernel\n");

  return 0;
}

static int update_xen(unsigned long phys_shdw_mem, unsigned int no_pages)
{
  unsigned long *mfn = kmalloc(sizeof(unsigned long) * no_pages, GFP_ATOMIC);
  unsigned long pfn;
  int i;
  char *c = text_start;
  struct mmu_update *mmu_updates = kmalloc(sizeof(struct mmu_update) *
                                            no_pages, GFP_ATOMIC);
  struct mmu_update *virt_updates = kmalloc(sizeof(struct mmu_update) *
                                             no_pages, GFP_ATOMIC);

  if (mfn == NULL) {
    return -ENOMEM;
  }
  if (mmu_updates == NULL) {
    kfree(mfn);
    return -ENOMEM;
  }
  if (virt_updates == NULL) {
    kfree(mfn);
    kfree(mmu_updates);
    return -ENOMEM;
  }

  for (i = 0; i < no_pages; i++) {
    pfn = (__pa(text_start) >> PAGE_SHIFT) + i;
    mfn[i] = virt_to_mfn(__va((char *)phys_shdw_mem) + PAGE_SIZE * i);
    mmu_updates[i].ptr = ((uint64_t)mfn[i] << PAGE_SHIFT) | MMU_MACHPHYS_UPDATE;
    mmu_updates[i].val = pfn;

    virt_updates[i].ptr = ((uint64_t)mfn[i] << PAGE_SHIFT) | MMU_NORMAL_PT_UPDATE;
    virt_updates[i].val = pfn;

    // Update p2m
    if (!KPRIV(__set_phys_to_machine)(pfn, mfn[i])) {
      WARN(1, "Failed to set p2m mapping for pfn=%ld mfn=%ld\n", pfn, mfn);
      goto err;
    }
  }
  /* Update m2p */
  if (HYPERVISOR_mmu_update(mmu_updates, no_pages, NULL, DOMID_SELF) < 0) {
    WARN(1, "Failed to set m2p mapping");
    goto err;
  }

  /* Update kernel mapping, but not for highmem. */
  if (pfn >= PFN_UP(__pa(high_memory - 1))) {
    goto out;
  }

  if (HYPERVISOR_mmu_update(virt_updates, no_pages, NULL, DOMID_SELF) < 0) {
    WARN(1, "Failed to set m2p mapping.");
    goto err;
  }

out:
  kfree(mfn);
  kfree(mmu_updates);
  kfree(virt_updates);
  return 0;

err:
  kfree(mfn);
  kfree(mmu_updates);
  kfree(virt_updates);
  return -1;
}

int shdw_switch_pages(shdw_hdl hdl, int num_pages)
{
  unsigned long phys_shdw_mem;

  if (!is_vm()) {
    printk(KERN_ERR "Cannot switch to a shadow kernel without a hypervisor\n");
    return -EINVAL;
  }

  if (hdl > no_shdws) {
    printk(KERN_ERR "Bad shadow hdl: %d\n", hdl);
    return -EINVAL;
  }

  phys_shdw_mem = phys_texts[hdl];
  update_xen(phys_shdw_mem, num_pages);
  current_shdw = hdl;
  return 0;
}

int shdw_switch(shdw_hdl hdl)
{
  return shdw_switch_pages(hdl, TEXT_PAGES);
}

int shdw_reset(void)
{
  if (current_shdw) {
    return shdw_switch(0);
  }
  else {
    return 0;
  }
}

int do_shdw_op(shdw_op op, shdw_hdl *shdw)
{
  pid_acct *current_pid_acct = CPU_VAR(current_acct);
  syscall_interest_t *interest;
  int rc;

  if (is_vm()) {
    if ((current_pid_acct != NULL) && (current_pid_acct->ctrl != NULL)) {
      interest = &current_pid_acct->ctrl->interest;
    }
    switch(op) {
      case NOP:
        break;
      case SPAWN_ONLY:
      case SPAWN_SWAP_ON_SCHED:
        rc = shdw_create(shdw);
        if (rc) {
          printk(KERN_ERR "Unable to create a shadow kernel.\n");
          return rc;
        }
        if (op == SPAWN_SWAP_ON_SCHED) {
          current_pid_acct->shdw_kernel = *shdw;
        }
        break;
      case SWAP:
        if (interest->shdw_pages) {
          return shdw_switch_pages(interest->use_shdw, interest->shdw_pages);
        } else {
          return shdw_switch(interest->use_shdw);
        }
      default:
        break;
    }
    return 0;
  }
  return -EINVAL;
}
