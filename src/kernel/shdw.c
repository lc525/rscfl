#include "rscfl/kernel/shdw.h"

#include <asm/page.h>
#include <asm/pgtable_types.h>
#include <asm/xen/hypercall.h>
#include <asm/xen/page.h>
// Needs to be here as pgtable.h depends on spinlock_types.h. *sigh*
#include <linux/spinlock_types.h>
//#include <asm/pgtable.h>
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
#include "rscfl/kernel/chardev.h"
#include "rscfl/kernel/priv_kallsyms.h"

#define MAX_SHDW_KERNELS 10
#define NUM_PTES 10

unsigned int no_shdws = 0;
DEFINE_SPINLOCK(shdw_lock);

static unsigned long phys_texts[MAX_SHDW_KERNELS];

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

shdw_hdl shdw_create(void)
{
  int shdw_no;
  char *shdw_mem;
  struct page *page;
  pte_t pte;

  text_start = (char *)KPRIV(_text);
  text_end = (char *)KPRIV(_etext);
  text_size = text_end - text_start;

  debugk("Text start=%p\n", text_start);
  debugk("Text end=%p\n", text_end);
  debugk("Text size=%d\n", text_size);

  if (shdw_no >= MAX_SHDW_KERNELS) {
    return -ENOMEM;
  }

  spin_lock(&shdw_lock);
  shdw_no = ++no_shdws;
  spin_unlock(&shdw_lock);

  // Allocate memory for the new kernel text section. This will be contiguous in
  // physical memory.

  page = KPRIV(dma_alloc_from_contiguous)(rscfl_ctrl_device,
                                          text_size / PAGE_SIZE + 1, 0);

  shdw_mem = __va((char *)page_to_phys(page));

  debugk(KERN_ERR "Shdw_mem at %p\n", shdw_mem);

  if (shdw_mem == NULL) {
    return -ENOMEM;
  }

  // Copy the current kernel text section into this new memory.
  memcpy(shdw_mem, text_start, text_size);

  // Store the physical address of the shadow memory.
  phys_texts[shdw_no] = __pa(shdw_mem);

  return shdw_no;
}

static void xen_update_mem_tables(unsigned long pfn, unsigned long mfn)
{
  struct mmu_update update = {
      .ptr = ((uint64_t)mfn << PAGE_SHIFT) | MMU_MACHPHYS_UPDATE,
      .val = pfn & ~((unsigned long)1)};

  /* Update p2m */
  if (!KPRIV(__set_phys_to_machine)(pfn, mfn)) {
    WARN(1, "Failed to set p2m mapping for pfn=%ld mfn=%ld\n", pfn, mfn);
  }

  /* Update m2p */
  if (HYPERVISOR_mmu_update(&update, 1, NULL, DOMID_SELF) < 0) {
    WARN(1, "Failed to set m2p mapping for mfn=%ld pfn=%ld\n", mfn, pfn);
  }

  /* Update kernel mapping, but not for highmem. */
  if (pfn >= PFN_UP(__pa(high_memory - 1))) {
    return;
  }

  if (HYPERVISOR_update_va_mapping((unsigned long)__va(pfn << PAGE_SHIFT),
                                   mfn_pte(mfn, PAGE_KERNEL_EXEC), 0)) {
    WARN(1, "Failed to update kernel mapping for mfn=%ld pfn=%ld\n", mfn, pfn);
  }
}

static int update_xen(void *var)
{
  unsigned long phys_shdw_mem = *((unsigned long *)var);
  unsigned long mfn;
  int i;
  char *c;
  /* c = __va(page_to_phys(KPRIV(dma_alloc_from_contiguous)( */
  /*     rscfl_ctrl_device, text_size / PAGE_SIZE + 1, 0))); */
  c = text_start;
  debugk(KERN_ERR "mem at %p\n", c);
  for (i = 16; i < 30; i++) {
    debugk(KERN_ERR "Iter %d\n", i);
    mfn = virt_to_mfn(__va((char *)phys_shdw_mem) + PAGE_SIZE * i);
    xen_update_mem_tables((__pa(c) >> PAGE_SHIFT) + i, mfn);
  }
  return 0;
}

int shdw_switch(shdw_hdl hdl)

{
  unsigned long phys_shdw_mem;
  int i;

  if (hdl > no_shdws) {
    return -EINVAL;
  }

  phys_shdw_mem = phys_texts[hdl];
  debugk(KERN_ERR "Swapping to shadow at %p\n", __va(phys_shdw_mem));
  unmap_kernel_range((unsigned long)__va(phys_shdw_mem),
                     ((text_size / 4096) + 1) * 4096 );
  debugk(KERN_ERR "Changing kernel page tables to be RW.\n");
  write_cr0(read_cr0() & (~0x10000));

  stop_machine(update_xen, &phys_shdw_mem, NULL);
  //update_xen(phys_shdw_mem);

  debugk(KERN_ERR "Remapped\n");
  write_cr0(read_cr0() | (0x10000));

  return 0;
}
