#include <errno.h>
#include <stdio.h>
#include <sys/socket.h>
#include <rscfl/user/res_api.h>
#include <rscfl/costs.h>

int main(int argc, char *argv[])
{
  int DEBUG = 0;
  int sock_domain = AF_LOCAL;
  int sock_type = SOCK_RAW;
  int sock_proto = 0;

  int socfd_1;
  int socfd_2;
  int socfd_3;

  if (DEBUG) printf("Opening sockets\n");

  rscfl_handle relay_f_data;
  struct accounting acct = {0};
  struct accounting acct2 = {0};

  if (!(relay_f_data = rscfl_init())) {
    fprintf(stderr, "rscfl: Error initialising. Errno=%d\n", errno);
    return -1;
  }

  // Open 3 sockets
  if ((socfd_1 = socket(sock_domain, sock_type, sock_proto)) < 0) {
    goto soc_err;
  }

  if (rscfl_acct_next(relay_f_data)) {
    fprintf(stderr, "rscfl: acct_next errno=%d\n", errno);
    return -1;
  }

  if ((socfd_2 = socket(sock_domain, sock_type, sock_proto)) < -1) {
    goto soc_err;
  }

  if (!rscfl_read_acct(relay_f_data, &acct)) {
    printf("rscfl: cpu_cycles=%llu wall_clock_time=%llu\n", acct.cpu.cycles,
           acct.cpu.wall_clock_time);
  } else {
    fprintf(stderr, "rscfl: read_acct failed\n");
  }

  if (rscfl_acct_next(relay_f_data)) {
    fprintf(stderr, "rscfl 2: acct_next errno=%d\n", errno);
    return -1;
  }

  if ((socfd_3 = socket(sock_domain, sock_type, sock_proto)) < -1) {
    goto soc_err;
  }

  if (!rscfl_read_acct(relay_f_data, &acct2)) {
    printf(
        "rscfl 2: cpu_cycles=%llu\n"
        "wall_clock_time=%llu\n"
        "page_faults=%llu\n"
        "align_faults=%llu\n",
        acct2.cpu.cycles, acct2.cpu.wall_clock_time, acct2.mem.page_faults,
        acct2.mem.align_faults);
  } else {
    fprintf(stderr, "rscfl: read_acct failed\n");
  }

  if (DEBUG) printf("Closing sockets\n");

  // Be a good citizen
  close(socfd_1);
  close(socfd_2);
  close(socfd_3);
  return 0;

soc_err:
  fprintf(stderr, "rscfl: error creating sockets\n");
  return -1;
}
