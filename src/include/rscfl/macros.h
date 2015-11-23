#ifndef _RSCFL_MACROS_H_
#define _RSCFL_MACROS_H_

#ifndef NDEBUG
#define debugk(format, ...) printk(format, ##__VA_ARGS__)
#else
#define debugk(format, ...)
#endif

#ifdef UNUSED
#elif defined(__GNUC__)
  #define UNUSED(x) UNUSED_##x __attribute__((unused))
#elif defined(__LCLINT__)
  #define UNUSED(x) /*@unused@*/ x
#else
  #define UNUSED(x) x
#endif

#define NOT(x) 1-x

#define EBIT(x) 1U << x
#define ALL_EBITS(x) (1U << (x + 1)) - 1

#ifndef __KERNEL__
#define likely(x)       __builtin_expect(!!(x), 1)
#define unlikely(x)     __builtin_expect(!!(x), 0)
#endif

#endif // _RSCFL_MACROS_H_
