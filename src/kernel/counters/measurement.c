#include "rscfl/kernel/measurement.h"

#include "linux/nmi.h"
#include "linux/time.h"

#include "rscfl/kernel/cpu.h"
#include "rscfl/kernel/perf.h"

/*
 * Some extra, useful counters
 */
static __inline__ ru64 rscfl_get_cycles(void)
{
  unsigned int hi, lo;
  __asm__ volatile("rdtsc" : "=a"(lo), "=d"(hi));
  return ((ru64)hi << 32) | lo;
}

static struct timespec rscfl_get_timestamp(void)
{
  struct timespec ts;
  getrawmonotonic(&ts);
  return ts;
}

int rscfl_counters_init(void)
{
  return rscfl_perf_init();
}

void rscfl_counters_stop(void)
{
  rscfl_perf_stop();
}

int rscfl_counters_update_subsys_vals(struct subsys_accounting *add_subsys,
                                      struct subsys_accounting *minus_subsys)
{
  u64 cycles = rscfl_get_cycles();
  struct timespec time = rscfl_get_timestamp();

  if (add_subsys != NULL) {
    add_subsys->cpu.cycles += cycles;
    add_subsys->cpu.wall_clock_time.tv_sec += time.tv_sec;
    add_subsys->cpu.wall_clock_time.tv_nsec += time.tv_nsec;
  }

  if (minus_subsys != NULL) {
    minus_subsys->cpu.cycles -= cycles;
    minus_subsys->cpu.wall_clock_time.tv_sec -= time.tv_sec;
    minus_subsys->cpu.wall_clock_time.tv_nsec -= time.tv_nsec;
  }

  return rscfl_snapshot_perf(add_subsys, minus_subsys);
}

