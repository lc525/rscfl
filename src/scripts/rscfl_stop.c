/**** Notice
 * rscfl_stop.c: rscfl source code
 *
 * Copyright 2015-2017 The rscfl owners <lucian.carata@cl.cam.ac.uk>
 *
 * This file is part of the rscfl open-source project: github.com/lc525/rscfl;
 * Its licensing is governed by the LICENSE file at the root of the project.
 **/

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
