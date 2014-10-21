#include "rscfl/kernel/cpu.h"

#include <linux/percpu.h>

#include "rscfl/config.h"
#include "rscfl/kernel/hasht.h"
#include "rscfl/res_common.h"

DEFINE_PER_CPU(u32, current_pid_ix);
DEFINE_PER_CPU(htbl, pid_acct_htbl);

int _rscfl_cpus_init(void){
  int cpu_id;
  int errg = 0, errc = 0;

  for_each_present_cpu(cpu_id) {
//    debugk("[cpu %d]: initializing hashtable\n", cpu_id);
    errc = htbl_init(&per_cpu(pid_acct_htbl, cpu_id), CPU_PIDACCT_HTBL_LOGSIZE);
    errg |= errc;
  }
  return errg;
}

int _rscfl_cpus_cleanup(void){
  int cpu_id;
  for_each_present_cpu(cpu_id) {
//    debugk("[cpu %d]: cleaning hashtable\n", cpu_id);
    htbl_clear(&per_cpu(pid_acct_htbl, cpu_id));
  }
  return 0;
}

