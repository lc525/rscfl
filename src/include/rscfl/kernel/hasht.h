#ifndef _RSCFL_HASH_H_
#define _RSCFL_HASH_H_
/**
 * Defines macros needed for defining per-cpu hash tables.
 * Uses the very simple linux hashtable implementation in linux/hashtable.h
 */

#include <linux/hashtable.h>
#include <linux/list.h>


/* DE{FIN,CLAR}E_PER_CPU_HASHTABLE macro notes:
 * 1. see include/linux/percpu-defs.h for possible further optimisations
 *    e.g using DEFINE_PER_CPU_SHARED_ALIGNED instead of DEFINE_PER_CPU
 * 2. the { [0...x] } expression uses the designated initializers gcc extension
 *    http://gcc.gnu.org/onlinedocs/gcc-4.1.2/gcc/Designated-Inits.html
 */
#define DEFINE_PER_CPU_HASHTABLE(tbl_name, bits)                               \
  DEFINE_PER_CPU(struct hlist_head[1 << (bits)], tbl_name) =                   \
            { [0 ... ((1 << (bits)) - 1)] = HLIST_HEAD_INIT }

#define DECLARE_PER_CPU_HASHTABLE(tbl_name, bits)                              \
  DECLARE_PER_CPU(struct hlist_head[1 << (bits)], tbl_name)


/*
 * If preemption is disabled, you can access per-cpu variables directly
 * with *this_cpu_ptr(&var); However, in other contexts one must use the
 * get_cpu_var/put_cpu_var macros (you don't want to be preempted in the middle
 * of accessing cpu variables as you might get moved on a different CPU).
 *
 * CPU_HTBL_DO is a convenience macro for doing hashtable operations from
 * a context where preemption is enabled.
 *
 * See http://www.makelinux.net/books/lkd2/ch09lev1sec9 for a better solution
 * when using per-cpu variables frequently within a block or loop
 */
#define CPU_HTBL_DO(fct, name, ...) do {                                       \
                                      fct(get_cpu_var(name), __VA_ARGS__);     \
                                      put_cpu_var(name);                       \
                                    } while(0)

/*
 * ONLY use this when preemption is disabled. If you don't do this and you
 * are scheduled (possibly on a different CPU), you're going to have a bad time.
 */
#define CPU_TBL(tbl_name) (*(this_cpu_ptr(&tbl_name)))

#endif
