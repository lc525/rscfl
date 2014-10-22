#include "rscfl/kernel/hasht.h"

#include "rscfl/PMurHash.h"
#include "rscfl/res_common.h"

short htbl_init(htbl* hasht, int logsize)
{
  int size;
  if (hasht == NULL) goto error;
  hasht->logsize = logsize;
  size = 1 << logsize;
  hasht->mask = size - 1;
  // shoud this be a vmalloc instead?
  if (!(hasht->table = ((hitem*)kmalloc(sizeof(hitem) * size, GFP_ATOMIC)))) {
    //    printk("rscfl: could not allocate hash table %p\n", (void*)hasht);
    goto error;
  } else {
    //    debugk("hash initialized\n");
    return 0;
  }

error:
  return -1;
}

void htbl_add(htbl* hasht, pid_t pid, hitem value)
{
  unsigned int ix;
  ix = PMurHash32(MUR_HASH_SEED, &pid, 1);
  ix = ix & hasht->mask;
  if (unlikely(hasht->table[ix].pid != 0 && hasht->table[ix].pid != pid)) {
    printk("Hash Table %p collision!\n", (void*)hasht);
  }
  hasht->table[ix] = value;
}

hitem htbl_get(htbl* hasht, pid_t pid, unsigned int* ix_out)
{
  *ix_out = PMurHash32(MUR_HASH_SEED, &pid, 1);
  *ix_out = *ix_out & hasht->mask;
  return hasht->table[*ix_out];
}

void htbl_remove(htbl* hasht, pid_t pid)
{
  unsigned int ix;
  ix = PMurHash32(MUR_HASH_SEED, &pid, 1);
  ix = ix & hasht->mask;
  if (hasht->table[ix].pid == pid) {
    hasht->table[ix].pid = 0;
    hasht->table[ix].curr_acct = 0;
  }
}

void htbl_clear(htbl* hasht)
{
  if (hasht != NULL) {
    kfree(hasht->table);
  }
}
