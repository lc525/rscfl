#include <errno.h>
#include <fcntl.h>
#include "gtest/gtest.h"
#include <stdio.h>
#include <sys/socket.h>
#include <time.h>

#include <rscfl/costs.h>
#include <rscfl/subsys_list.h>
#include <rscfl/user/res_api.h>

class WCTTest : public testing::Test
{
 protected:
  virtual void SetUp()
  {
    rhdl_ = rscfl_init();
    ASSERT_NE(nullptr, rhdl_);
  }

  rscfl_handle rhdl_;
};

static struct timespec wct_test_get_time(void)
{
  struct timespec ts;
  clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &ts);
  return ts;
}

/*
 * Tests whether the number of cycles seen in the kernel is less than that seen
 * in user space. when opening a socket.
 */
TEST_F(WCTTest, WallClock_Kernel_LT_User)
{
  ASSERT_EQ(0, rscfl_acct_next(rhdl_));

  struct timespec val_pre = wct_test_get_time();
  int sockfd = socket(AF_LOCAL, SOCK_RAW, 0);
  EXPECT_GT(sockfd, 0);
  struct timespec val_post = wct_test_get_time();
  ru64 user_time = (ru64)val_post.tv_nsec - val_pre.tv_nsec;

  struct accounting acct;
  ASSERT_EQ(0, rscfl_read_acct(rhdl_, &acct));

  // Now add all of the subsystem times
  ru64 kernel_time = 0;
  struct subsys_accounting *subsys;
  rscfl_subsys curr_sub;
  for (int i = 1; i < NUM_SUBSYSTEMS; i++) {
    if ((subsys = get_subsys_accounting(rhdl_, &acct, (rscfl_subsys)i)) !=
        NULL) {
      kernel_time += (ru64)subsys->cpu.wall_clock_time.tv_nsec;
    }
  }

  EXPECT_LT(kernel_time, user_time);
}
