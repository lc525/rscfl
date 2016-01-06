#ifndef _RSCFL_XEN_H_
#define _RSCFL_XEN_H_

#include "rscfl/costs.h"

#ifdef _RSCFL_XEN_IMPL_
  #define _once
#else
  #define _once extern
#endif

#define NUM_XEN_PAGES 10
#define XEN_EVENTS_PER_PAGE 50

// TODO(oc243): We currently only use 256 events rather than all of the
// available ones, due to Xen's setup.
#define CURRENT_XEN_NUM_EVENTS 256

/*
 * Data structures shared by rscfl-enabled xen
 */
struct sched_event
{
  uint64_t cycles;
  uint8_t credit;

  _Bool is_yield;

  _Bool is_block;
  _Bool is_unblock;

  _Bool sched_in;
  _Bool sched_out;
};
typedef struct sched_event sched_event_t;

struct shared_sched_info
{
  uint64_t sched_out;
  uint8_t sched_tl;
  uint8_t sched_hd;
  unsigned long rscfl_page_phys[NUM_XEN_PAGES];
};

_once short disable_xen;
_once ru64 no_evtchn_events;
_once char *rscfl_pages[NUM_XEN_PAGES];

#undef _once

int xen_scheduler_init(void);
int xen_buffer_hd(void);
uint64_t xen_current_sched_out(void);
void xen_clear_current_sched_out(void);

#endif
