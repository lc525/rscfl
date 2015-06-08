#ifndef _RSCFL_PRIV_KALLSYMS_H_

#include <asm/pgtable_types.h>
#include <linux/mutex.h>
#include <linux/tracepoint.h>
#include <linux/types.h>

#define KPRIV(name) name##_ptr

#ifdef _PRIV_KALLSYMS_IMPL_
  #define _once
#else
  #define _once extern
#endif

#define PRIV_KSYM_TABLE(_) \
_(text_poke)               \
_(text_mutex)              \
_(__vmalloc_node_range)    \
_(HYPERVISOR_shared_info)  \
_(xen_dummy_shared_info)   \
_(xen_evtchn_do_upcall)    \
_(set_pte_vaddr)           \
_(__set_phys_to_machine)   \
_(put_balloon_scratch_page)\
_(get_balloon_scratch_page)\
_(flush_tlb_all)           \

_once void* (*KPRIV(text_poke))(void *addr, const void *opcode, size_t len);
_once void (*KPRIV(xen_evtchn_do_upcall))(struct pt_regs *regs);
_once struct mutex *KPRIV(text_mutex);
_once void* (*KPRIV(__vmalloc_node_range))(
    unsigned long size, unsigned long align, unsigned long start,
    unsigned long end, gfp_t gfp_mask, pgprot_t prot, int node,
    const void *caller);

_once struct shared_info **KPRIV(HYPERVISOR_shared_info);
_once struct shared_info *KPRIV(xen_dummy_shared_info);
_once void (*KPRIV(set_pte_vaddr))(unsigned long vaddr, pte_t pte);
_once bool (*KPRIV(__set_phys_to_machine))(unsigned long pfn, unsigned long mfn);
_once void (*KPRIV(put_balloon_scratch_page))(void);
_once struct page* (*KPRIV(get_balloon_scratch_page))(void);
_once void (*KPRIV(flush_tlb_all))(void);

int init_priv_kallsyms(void);


#endif /* _RSCFL_PRIV_KALLSYMS_H_ */
