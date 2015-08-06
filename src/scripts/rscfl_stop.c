#include "rscfl/user/res_api.h"

#include <sys/ioctl.h>

int main(int argc, char** argv) {
  int rc = 1;
  rscfl_handle hdl = rscfl_get_handle();
  if (hdl != NULL) {
    rc = ioctl(hdl->fd_ctrl, RSCFL_SHUTDOWN_CMD);
  }
  return rc;
}
