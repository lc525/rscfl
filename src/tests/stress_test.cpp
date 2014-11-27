#include <errno.h>
#include "gtest/gtest.h"
#include <stdio.h>
#include <sys/socket.h>

#include <rscfl/costs.h>
#include <rscfl/user/res_api.h>

class StressTest : public testing::Test
{

 protected:
  virtual void SetUp()
  {
    rhdl_ = rscfl_init();
    ASSERT_NE(nullptr, rhdl_);
  }

  rscfl_handle rhdl_;
  struct accounting acct_;
};


// Try creating, and closing 1000 sockets, and accounting for all syscalls.
// We use ASSERTS as when the test fails with an EXPECT, we don't want to flood
// stdout.
TEST_F(StressTest, TestAcctForAThousandSocketOpens)
{
  int sock_fd;
  for (int i = 0; i < 1000; i++) {
    // Account for opening a socket.
    ASSERT_EQ(0, rscfl_acct_next(rhdl_));
    sock_fd = socket(AF_LOCAL, SOCK_RAW, 0);
    ASSERT_GT(sock_fd, 0);
    // Ensure that we are able to read back the struct accounting.
    ASSERT_EQ(0, rscfl_read_acct(rhdl_, &acct_))
        << "Failed at accounting for socket creation at attempt" << i;
    rscfl_subsys_free(rhdl_, &acct_);

    // Account for closing the socket again.
    ASSERT_EQ(0, rscfl_acct_next(rhdl_));
    close(sock_fd);
    ASSERT_EQ(0, rscfl_read_acct(rhdl_, &acct_))
        << "Failed at closing socket at attempt " << i;
    rscfl_subsys_free(rhdl_, &acct_);
  }
}
