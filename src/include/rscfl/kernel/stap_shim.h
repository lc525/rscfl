#ifndef _KO_RESOURCEFUL_H_
#define _KO_RESOURCEFUL_H_

#include "rscfl/costs.h"

#define FILL_MM 0
#define FILL_NET 1
#define FILL_FS 2
#define FILL_GLOBAL 3

struct syscall_acct_list_t
{
  unsigned long syscall_id;
  pid_t pid;
  int syscall_nr;
  struct syscall_acct_list_t *next;
};
typedef struct syscall_acct_list_t syscall_acct_list_t;

struct rscfl_pid_pages_t
{
  char *buf;
  struct rscfl_pid_pages_t *next;
  pid_t pid;
};
typedef struct rscfl_pid_pages_t rscfl_pid_pages_t;

static long syscall_id_c;
static syscall_acct_list_t *syscall_acct_list;
static rscfl_pid_pages_t *rscfl_pid_pages;

int acct_next(pid_t, int);
int _should_acct(pid_t pid, int syscall_nr, struct accounting **,
                 int probe_nest, const char *name);
int _fill_struct(long, long, struct accounting *, long);
int _clear_acct_next(pid_t, int);

#endif
