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

    virtual void TearDown()
    {
      close(sockfd_);
    }

    rscfl_handle rhdl_;
    int sockfd_;
};

time_t wct_test_get_time(void)
{
  struct timespec ts = {0};
  clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &ts);
  return ts.tv_nsec;
}

/*
 * Tests whether the number of cycles seen in the kernel is less than that seen
 * in user space. when opening a socket.
 */
TEST_F(WCTTest, WallClockValidation)
{
  ASSERT_EQ(0, rscfl_acct_next(rhdl_));

  ru64 val_pre = (ru64) wct_test_get_time();
  int sockfd_ = socket(AF_LOCAL, SOCK_RAW, 0);
  ru64 val_post = (ru64) wct_test_get_time();
  ru64 user_time = val_post - val_pre;

  struct accounting acct_;
  ASSERT_EQ(0, rscfl_read_acct(rhdl_, &acct_));

  // Now add all of the subsystem times
  ru64 kernel_time = 0;
  struct subsys_accounting *subsys;
  rscfl_subsys curr_sub;
  for (int i = 1; i < NUM_SUBSYSTEMS; i++) {
    curr_sub = (rscfl_subsys) i;
    if ((subsys = get_subsys_accounting(rhdl_, &acct_, curr_sub)) != NULL) {
      kernel_time += subsys->cpu.wall_clock_time;
    }
  }

  EXPECT_LT(kernel_time, user_time);
}

