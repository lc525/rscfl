#ifndef _RES_COMMON_H_
#define _RES_COMMON_H_

#define RSCFL_DRIVER "rscfl"
#define MMAP_BUF_SIZE 4096  // need to think about this

#ifndef NDEBUG
#define debugk(format, ...) printk(format, ##__VA_ARGS__)
#else
#define debugk(format, ...)
#endif

#endif
