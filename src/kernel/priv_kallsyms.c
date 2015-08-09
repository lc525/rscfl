#define _PRIV_KALLSYMS_IMPL_
#include "rscfl/kernel/priv_kallsyms.h"

#include <linux/kallsyms.h>

#include "rscfl/res_common.h"

#define XSTR(s) STR(s)
#define STR(s) #s


#define KSYM_INIT(ksym_name)                                        \
{                                                                   \
  KPRIV(ksym_name) = (void *)kallsyms_lookup_name(XSTR(ksym_name)); \
  if(KPRIV(ksym_name) == NULL){                                     \
    printk(KERN_ERR XSTR(ksym_name) ": not found\n");               \
    symbols_not_found++;                                            \
  } else {                                                          \
    debugk(KERN_NOTICE XSTR(ksym_name) ": %p\n", KPRIV(ksym_name)); \
  }                                                                 \
}                                                                   \

int init_priv_kallsyms(void)
{
  int symbols_not_found = 0;
  PRIV_KSYM_TABLE(KSYM_INIT);
  if(symbols_not_found)
    return -ENOSYS;
  else
    return 0;
}
