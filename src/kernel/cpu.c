/**** Notice
 * cpu.c: rscfl source code
 *
 * Copyright 2015-2017 The rscfl owners <lucian.carata@cl.cam.ac.uk>
 *
 * This file is part of the rscfl open-source project: github.com/lc525/rscfl;
 * Its licensing is governed by the LICENSE file at the root of the project.
 **/

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
  int cpu_id;
  int bkt;
  pid_acct *it;

  CPU_VAR(current_acct) = NULL;
  for_each_present_cpu(cpu_id) {
    hash_for_each(per_cpu(pid_acct_tbl, cpu_id), bkt, it, link[cpu_id]) {
      hash_del(&it->link[cpu_id]);
    }
  }
  return 0;
}
