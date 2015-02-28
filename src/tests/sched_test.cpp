#include "gtest/gtest.h"
#include <stdio.h>
#include <sys/socket.h>

#include <rscfl/costs.h>
#include <rscfl/res_common.h>
#include <rscfl/user/res_api.h>

class SchedTest : public testing::Test
{
 protected:
  virtual void SetUp()
  {
    // We must be able to initialise resourceful.
    rhdl_ = rscfl_init();
    ASSERT_NE(nullptr, rhdl_);

    // We must be able to account next.
    struct timespec start_time;
    clock_gettime(CLOCK_MONOTONIC_RAW, &start_time);

    ASSERT_EQ(0, rscfl_acct_next(rhdl_));
    sockfd_ = socket(AF_LOCAL, SOCK_RAW, 0);

    // If we can't initialise a socket, something has gone wrong.
    EXPECT_GT(sockfd_, 0);
    clock_gettime(CLOCK_MONOTONIC_RAW, &run_time_);

    // We must be able to read our struct accounting back from rscfl.
    ASSERT_EQ(0, rscfl_read_acct(rhdl_, &acct_));

    // Make sure we can read the subsystems
    sub_set_ = rscfl_get_subsys(rhdl_, &acct_);
    ASSERT_TRUE(sub_set_ != NULL);

    rscfl_timespec_diff(&run_time_, &start_time);
  }

  virtual void TearDown()
  {
    close(sockfd_);
    free_subsys_idx_set(sub_set_);
  }

  rscfl_handle rhdl_;
  struct accounting acct_;
  int sockfd_;
  subsys_idx_set *sub_set_;
  struct timespec run_time_;
};

/*
 * Test: Make sure we never get a negative sched time
 */
TEST_F(SchedTest, SchedTimeAlwaysPositive)
{
  for (int i = 0; i < sub_set_->set_size; i++) {
    ASSERT_GE(sub_set_->set[i].sched.wct_out_local.tv_sec, 0);
    ASSERT_GE(sub_set_->set[i].sched.wct_out_local.tv_nsec, 0);
  }
}

/*
 * Test: Ensure the time scheduled out is less than the total time the subsystem
 * was active for
 */
TEST_F(SchedTest, SchedTimeLessThanSubsysTime)
{
  for (int i = 0; i < sub_set_->set_size; i++) {
    ASSERT_EQ(-1, rscfl_timespec_compare(&sub_set_->set[i].sched.wct_out_local,
                                         &sub_set_->set[i].cpu.wall_clock_time));
  }
}

/*
 * Test: Ensure we never see a longer schedule period larger than the total time
 * taken to execute the call
 */
TEST_F(SchedTest, SchedTotalTimeGreater)
{
  struct timespec k_sched_time = {0, 0};
  for (int i = 0; i < sub_set_->set_size; i++) {
    rscfl_timespec_add(&k_sched_time, &sub_set_->set[i].sched.wct_out_local);
  }

  EXPECT_EQ(-1, rscfl_timespec_compare(&k_sched_time, &run_time_));
}
