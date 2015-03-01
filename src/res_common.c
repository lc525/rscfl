#include "rscfl/res_common.h"

/*
 * Shared timespec code
 */
void rscfl_timespec_add(struct timespec *to, const struct timespec *from) {
  to->tv_sec += from->tv_sec;
  to->tv_nsec += from->tv_nsec;

  // 1s = 1e9 ns; if tv_nsec is above 1s, then we have to add that to the
  // seconds field (tv_sec) and reduce tv_nsec accordingly
#ifdef __KERNEL__
  if (to->tv_nsec > 1000000000) {
    to->tv_nsec -= 1000000000;
#else
  if (to->tv_nsec > 1e9) {
    to->tv_nsec -= 1e9;
#endif
    to->tv_sec++;
  }
}

void rscfl_timespec_add_ns(struct timespec *ts, ru64 ns) {
  int sec = ns/1000000000;
  ns=ns - sec*1000000000;

  // perform the addition
  ts->tv_nsec+=ns;

  // adjust the time
  ts->tv_sec+=ts->tv_nsec/1000000000 + sec;
  ts->tv_nsec=ts->tv_nsec%1000000000;
}

// per-component timespec diff
// this substracts each of the timespec components of ex from the corresponding
// timespec components of from "from - ex"
//
// Use this instead of rscfl_timespec_diff when you want to accept negative
// results (seconds and/or nanoseconds)
//
void rscfl_timespec_diff_comp(struct timespec *from, const struct timespec *ex) {
    from->tv_sec = from->tv_sec - ex->tv_sec;
    from->tv_nsec = from->tv_nsec - ex->tv_nsec;
}

// timespec end will be updated to contain the duration between start and
// end; if end < start, end will be set to zero
void rscfl_timespec_diff(struct timespec *end, const struct timespec *start) {

  if ((end->tv_sec < start->tv_sec) ||
      ((end->tv_sec == start->tv_sec) &&
       (end->tv_nsec <= start->tv_nsec))) { /* end <= start? */
    end->tv_sec = end->tv_nsec = 0 ;
  } else {                                  /* end > start */
    end->tv_sec = end->tv_sec - start->tv_sec ;
    if (end->tv_nsec < start->tv_nsec) {
#ifdef __KERNEL__
      end->tv_nsec = end->tv_nsec + 1000000000 - start->tv_nsec;
#else
      end->tv_nsec = end->tv_nsec + 1e9 - start->tv_nsec;
#endif
      end->tv_sec--;                   /* borrow a second. */
    } else {
      end->tv_nsec = end->tv_nsec - start->tv_nsec ;
    }
  }
}

// returns -1 if time1 < time2
//          0 if time1 = time2
//          1 if time1 > time2
int rscfl_timespec_compare(struct timespec *time1, struct timespec *time2) {
  if (time1->tv_sec < time2->tv_sec)
    return (-1);                           /* less than. */
  else if (time1->tv_sec > time2->tv_sec)
    return (1);                            /* greater than. */
  else if (time1->tv_nsec < time2->tv_nsec)
    return (-1);                           /* less than. */
  else if (time1->tv_nsec > time2->tv_nsec)
    return (1);                            /* greater than. */
  else
    return (0);                            /* equal. */
}
