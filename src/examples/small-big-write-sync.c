#include <stdio.h>
#include <unistd.h>
#include <sys/fcntl.h>
#include "resourceful.h"

#define small 1
#define large 10


int main(int argc, char *argv[])
{
  int fd = open("file.bin", O_ASYNC | O_NONBLOCK);
  char small_data[small]; // Unitialized
  char large_data[large]; // Uninialized
  write(fd, small_data, small);
  write(fd, large_data, large);
  fsync(fd);
  close(fd);

  return 0;
}
