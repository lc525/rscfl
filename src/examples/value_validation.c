#include <errno.h>
#include <sys/types.h>
#include <netdb.h>
#include <stdio.h>
#include <sys/socket.h>
#include <rscfl/user/res_api.h>
#include <rscfl/costs.h>
#include <string.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>

int main(int argc, char *argv[])
{
	rscfl_handle r_handle;
	struct accounting acct_1 = {0};
  struct accounting acct_2 = {0};
	int sockfd_1;
	int sockfd_2;

	printf("Init rscfl\n");
	if (!(r_handle = rscfl_init())) {
		fprintf(stderr, "rscfl: Init error. Errno=%d\n", errno);
		return -1;
	}

  /*
   * SOCKET 1
   */
  printf("Opening socket 1: long distance\n");
	// Open the first socket and connect it to Australia
  if ((sockfd_1 = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    goto soc_err;

  struct sockaddr_in serv_addr_1;
  memset(&serv_addr_1, 0, sizeof(serv_addr_1));
  serv_addr_1.sin_family = AF_INET;
  serv_addr_1.sin_port = htons(80);
  if (inet_pton(AF_INET, "129.94.242.51", &serv_addr_1.sin_addr) <= 0) {
    fprintf(stderr, "inet_pton 1 error. Errno=%d\n", errno);
    return -1;
  }

  //RSCFL
  if (rscfl_acct_next(r_handle)) {
    fprintf(stderr, "rscfl: acct_next 1 errno=%d\n", errno);
    return -1;
  }

  if (connect(sockfd_1, (struct sockaddr *)&serv_addr_1, sizeof(serv_addr_1)) < 0) {
    fprintf(stderr, "Error connecting socket 1. Errno=%d\n", errno);
    return -1;
  }

	// RSCFL	
	if (!rscfl_read_acct(r_handle, &acct_1)) {
		printf("rscfl: cpu_cycles=%llu wall_clock_time=%llu\n", acct_1.cpu.cycles, acct_1.cpu.wall_clock_time);
	} else {
		fprintf(stderr, "rscfl: read_acct 1 failed\n");
    return -1;
	}


  /*
   * SOCKET 2
   */
  printf("Opening socket 2: short distance\n");
  // Open another socket and connect it to somewhere nearby
  if ((sockfd_2 = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    goto soc_err;

  struct sockaddr_in serv_addr_2;
  memset(&serv_addr_2, 0, sizeof(serv_addr_2));
  serv_addr_2.sin_family = AF_INET;
  serv_addr_2.sin_port = htons(80);
  if (inet_pton(AF_INET, "128.232.20.31", &serv_addr_2.sin_addr) <= 0) {
    fprintf(stderr, "inet_pton 2 error. Errno=%d\n", errno);
    return -1;
  }

  //RSCFL
  if (rscfl_acct_next(r_handle)) {
    fprintf(stderr, "rscfl: acct_next 2 errno=%d\n", errno);
    return -1;
  }

  if (connect(sockfd_2, (struct sockaddr *) &serv_addr_2, sizeof(serv_addr_2)) < 0) {
    fprintf(stderr, "Error connecting socket 2. Errno=%d\n", errno);
    return -1;
  }

  // RSCFL
  if (!rscfl_read_acct(r_handle, &acct_2)) {
    printf("rscfl: cpu_cycles=%llu wall_clock_time=%llu\n", acct_2.cpu.cycles, acct_2.cpu.wall_clock_time);
  } else {
    fprintf(stderr, "rscfl: read_acct 2 failed\n");
    return -1;
  }

	// Clean up
	close(sockfd_1);
  close(sockfd_2);
	return 0;

soc_err:
  fprintf(stderr, "Socket error. errno=%d\n", errno);
  return -1;
}
