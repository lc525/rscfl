#include <stdio.h>
#include <unistd.h>
#include <sys/fcntl.h>

#define small 1
#define large 10


int main(int argc, char *argv[])
{
  int fd = open("file.bin", O_ASYNC | O_NONBLOCK);
  char small_data[small]; // Unitialized
  char large_data[large]; // Uninialized
	if (write(fd, small_data, small) < 0) {
    fprintf(stderr, "Error writing\n");
		goto cleanup;
	}
	if (write(fd, large_data, large) < 0) {
    fprintf(stderr, "Error writing\n");
	}


cleanup:
  fsync(fd);
  close(fd);

  return 0;
}
