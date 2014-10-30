#include "rscfl/kernel/cpu.h"

#include <linux/percpu.h>

#include "rscfl/config.h"
#include "rscfl/kernel/hasht.h"
#include "rscfl/res_common.h"

DEFINE_PER_CPU(pid_acct*, current_acct);
DEFINE_PER_CPU_HASHTABLE(pid_acct_tbl, CPU_PIDACCT_HTBL_LOGSIZE);

int _rscfl_cpus_init(void)
{
  int cpu_id;
  for_each_present_cpu(cpu_id) {
    hash_init(per_cpu(pid_acct_tbl, cpu_id));
  }
  return 0;
}

int _rscfl_cpus_cleanup(void)
{
  return 0;
}
