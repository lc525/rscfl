#ifndef _RSCFL_PRIV_KALLSYMS_H_

#include <asm/pgtable_types.h>
#include <linux/mutex.h>
#include <linux/tracepoint.h>
#include <linux/types.h>

#include "rscfl/config.h"

#define KPRIV(name) name##_ptr

#ifdef _PRIV_KALLSYMS_IMPL_
  #define _once
#else
  #define _once extern
#endif

#define PRIV_KSYM_TABLE(_) \
_(HYPERVISOR_shared_info)  \
_(__set_phys_to_machine)   \
_(__vmalloc_node_range)    \
_(_etext)                  \
_(_text)                   \
_(flush_tlb_all)           \
_(get_balloon_scratch_page)\
_(put_balloon_scratch_page)\
_(set_pte_vaddr)           \
_(text_mutex)              \
_(text_poke)               \
_(xen_dummy_shared_info)   \
_(xen_evtchn_do_upcall)    \


#if SHDW_ENABLED != 0

#define PRIV_KSYM_SHDW_TABLE(_) \
_(dma_alloc_from_contiguous)    \
_(dma_contiguous_default_area)  \

#endif

_once struct shared_info **KPRIV(HYPERVISOR_shared_info);
_once bool (*KPRIV(__set_phys_to_machine))(unsigned long pfn, unsigned long mfn);
_once void* (*KPRIV(__vmalloc_node_range))(
    unsigned long size, unsigned long align, unsigned long start,
    unsigned long end, gfp_t gfp_mask, pgprot_t prot, int node,
    const void *caller);
_once char **KPRIV(_etext);
_once char **KPRIV(_text);
_once void (*KPRIV(flush_tlb_all))(void);
_once struct page* (*KPRIV(get_balloon_scratch_page))(void);
_once void (*KPRIV(put_balloon_scratch_page))(void);
_once void (*KPRIV(set_pte_vaddr))(unsigned long vaddr, pte_t pte);
_once struct mutex *KPRIV(text_mutex);
_once void* (*KPRIV(text_poke))(void *addr, const void *opcode, size_t len);
_once struct shared_info *KPRIV(xen_dummy_shared_info);
_once void (*KPRIV(xen_evtchn_do_upcall))(struct pt_regs *regs);

#if SHDW_ENABLED != 0
_once struct cma *KPRIV(dma_contiguous_default_area);
_once struct page *(*KPRIV(dma_alloc_from_contiguous))(struct device *dev,
    int count,
    unsigned int align);
#endif

#undef _once

int init_priv_kallsyms(void);

#endif /* _RSCFL_PRIV_KALLSYMS_H_ */
