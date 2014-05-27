#include <stdio.h>
#include <sys/socket.h>
#include <res_user/res_api.h>

int main(int argc, char *argv[])
{
  int DEBUG = 0;
  int sock_domain = AF_LOCAL;
  int sock_type = SOCK_RAW;
  int sock_proto = 0;

  int socfd_1;
  int socfd_2;
  int socfd_3;

  if (DEBUG)
    printf("Opening sockets\n");

  // Open 3 sockets
  socfd_1 = socket(sock_domain, sock_type, sock_proto);

  rscfl_init();
  rscfl_acct_next();
  socfd_2 = socket(sock_domain, sock_type, sock_proto);
  rscfl_read_acct();

  socfd_3 = socket(sock_domain, sock_type, sock_proto);

  if (DEBUG)
    printf("Closing sockets\n");

  // Be a good citizen
  close(socfd_1);
  close(socfd_2);
  close(socfd_3);
}
