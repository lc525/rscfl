#define BIT(x) 1U << x
#include "rscfl/costs.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>

#define RESOURCEFUL_FILE "/mnt/resourceful0"

int main(int argc, char *argv[])
{
  struct accounting *acc;
  const char *memblock;
  struct stat sb;
  int fd;
  int size;

  if ((fd = open(RESOURCEFUL_FILE, O_RDONLY)) == -1) {
    printf("Cannot open %s\n", RESOURCEFUL_FILE);
    return -1;
  }
  fstat(fd, &sb);
  size = sb.st_size;
  printf("size=%d\n", size);
  memblock = mmap((caddr_t)0, size, PROT_READ, 0, fd, 0);
  if (memblock == MAP_FAILED) {
    printf("Mmap failed: %d\n", errno);
    return errno;
  }

  while (1) {
    acc = (struct accounting *)memblock;
    printf("CPU cycles: %llu\n", acc->cpu.cycles);
    sleep(100);
  }

  return 0;
}
