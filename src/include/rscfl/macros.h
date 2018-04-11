/**** Notice
 * macros.h: rscfl source code
 *
 * Copyright 2015-2017 The rscfl owners <lucian.carata@cl.cam.ac.uk>
 *
 * This file is part of the rscfl open-source project: github.com/lc525/rscfl;
 * Its licensing is governed by the LICENSE file at the root of the project.
 **/

#ifndef _RSCFL_MACROS_H_
#define _RSCFL_MACROS_H_

#include "rscfl/config.h"

#ifndef NDEBUG
  #define debugk(level, format, ...) if(level <= DEBUG_LEVEL){ printk(format, ##__VA_ARGS__); }
#else
  #define debugk(level, format, ...)
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

/*
 * Used to implement API function overloading
 */

#define _ARG11(_0, _1, _2, _3, _4, _5, _6, _7, _8, _9, _10, ...) _10
#define HAS_NO_COMMA(...) _ARG11(__VA_ARGS__, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1)
#define _TRIGGER_ARGS(...) ,

#define HAS_ONE_ARG(...) HAS_NO_COMMA(_TRIGGER_ARGS __VA_ARGS__ (/* empty */))

#define VARGS_(_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, N, ...) N
#define VARGS_NR(...) VARGS_(__VA_ARGS__, 10, 9, 8, 7, 6, 5, 4, 3, 2, HAS_ONE_ARG(__VA_ARGS__), HAS_ONE_ARG(__VA_ARGS__))

#define CONCAT_(a, b) a##b
#define CONCAT(a, b) CONCAT_(a, b)




#endif // _RSCFL_MACROS_H_
