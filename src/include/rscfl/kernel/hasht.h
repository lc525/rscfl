/**
 * Implements simple hashtable with fast lookups.
 * The underlying hash function used is MurmurHash3 (32bit)
 *
 * We allocate one hash table per CPU in order to hold (pid, accounting*) pairs
 * for processes(pids) that are active (or have previously run) on that CPU but
 * have not terminated.
 *
 * TODO(lc525): deal with collisions. For now (testing) we just update the value
 * on collision (the hitem).
 */
#ifndef _RSCFL_HASH_H_
#define _RSCFL_HASH_H_

#include "rscfl/costs.h"

#define MUR_HASH_SEED 139423874

typedef struct
{
  pid_t pid;
  struct accounting* curr_acct;
} hitem;

typedef struct
{
  hitem* table;  // size: 2^logsize
  int logsize;
  unsigned int mask;  // hash value & mask = ix in table
} htbl;

short htbl_init(htbl* hasht, int logsize);
void htbl_add(htbl* hasht, pid_t pid, hitem value);
hitem htbl_get(htbl* hasht, pid_t pid, unsigned int* ix_out);
void htbl_remove(htbl* hasht, pid_t pid);
void htbl_clear(htbl* hasht);

#endif
