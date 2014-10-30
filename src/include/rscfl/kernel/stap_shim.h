#ifndef _KO_RESOURCEFUL_H_
#define _KO_RESOURCEFUL_H_

#include "rscfl/costs.h"
#include "rscfl/res_common.h"

struct syscall_acct_list_t
{
  unsigned long syscall_id;
  pid_t pid;
  int syscall_nr;
  struct syscall_acct_list_t *next;
};
typedef struct syscall_acct_list_t syscall_acct_list_t;

struct rscfl_shared_mem_layout_t
{
  struct accounting acct[STRUCT_ACCT_NUM];
  struct subsys_accounting subsyses[ACCT_SUBSYS_NUM];
};

typedef struct rscfl_shared_mem_layout_t rscfl_shared_mem_layout_t;

struct rscfl_pid_pages_t
{
  char *buf;
  struct rscfl_pid_pages_t *next;
  pid_t pid;
};
typedef struct rscfl_pid_pages_t rscfl_pid_pages_t;

extern long syscall_id_c;
extern syscall_acct_list_t *syscall_acct_list;
extern rscfl_pid_pages_t *rscfl_pid_pages;

int acct_next(pid_t, int);
int _should_acct(pid_t pid, int syscall_nr, int probe_nest, const char *name,
                 struct accounting **, rscfl_pid_pages_t **);
int _fill_struct(long, long, struct accounting *, long, rscfl_pid_pages_t *);
int _clear_acct_next(pid_t, int);

#endif
