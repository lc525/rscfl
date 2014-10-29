#include "rscfl/kernel/sched.h"

#include "rscfl/kernel/cpu.h"
#include "rscfl/kernel/hasht.h"

/* function that needs to be executed on every context switch
 * one extra hash table search on every context switch, for all processes
 */
void on_ctx_switch(pid_t next_tid) {
  pid_acct *it;

  hash_for_each_possible(CPU_TBL(pid_acct_tbl), it, link, next_tid)
    if(it->pid == next_tid){
      CPU_VAR(current_acct) = it;
      return;
    }

  // next_tid is not in the hash table
  CPU_VAR(current_acct) = NULL;
}


/* function that needs to be executed when a process/thread gets migrated from
 * cpu_from to cpu_to. Almost no overhead for processes that do not use
 * resourceful (just setting a kprobe)
 */
void on_cpu_switch(int cpu_from, int cpu_to, pid_t pid) {
  pid_acct *it;

  // Fastest path out: pid does not use resourceful
  if(CPU_VAR(current_acct) == NULL) return;

  /* We assume that if the process is long-lived, after a while all CPUs will
   * have its pid_acct structure within their hash table. So the fast path is to
   * just check for the existence of that entry and return. on_ctx_switch will
   * take care of the rest (updating current_acct on the new cpu)
   */
  hash_for_each_possible(per_cpu(pid_acct_tbl, cpu_to), it, link, pid)
    if(it->pid == pid){
      return;
    }

  /* The hash table of the CPU we're moving on doesn't have the required entry.
   * Add it; at this point, we know current_acct != NULL
   */
  hash_add(per_cpu(pid_acct_tbl, cpu_to), &CPU_VAR(current_acct)->link, pid);
}
