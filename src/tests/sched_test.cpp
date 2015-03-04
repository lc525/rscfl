#include "gtest/gtest.h"
#include <stdio.h>
#include <sys/socket.h>

#include <rscfl/costs.h>
#include <rscfl/res_common.h>
#include <rscfl/user/res_api.h>

const int kMaxExpectedCyclesSpentInHypervisorPerSubsystem = 100000;
const int kMaxExpectedSchedulesPerSubsystem = 16;


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
    run_cycles_ -= rscfl_get_cycles();

    ASSERT_EQ(0, rscfl_acct_next(rhdl_));
    sockfd_ = socket(AF_LOCAL, SOCK_RAW, 0);

    // If we can't initialise a socket, something has gone wrong.
    EXPECT_GT(sockfd_, 0);

    run_cycles_ += rscfl_get_cycles();
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
  ru64 run_cycles_;
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

TEST_F(SchedTest, SchedCyclesLessThanTotal)
{
  ru64 total_sched = 0;
  for (int i = 0; i < sub_set_->set_size; i++) {
    total_sched += sub_set_->set[i].sched.cycles_out_local;
  }
  ASSERT_LT(total_sched, run_cycles_);
}

TEST_F(SchedTest, HypervisorSchedulesDoesntOverflow)
{
  for (int i = 0; i < sub_set_->set_size; i++) {
    // Must not overflow or underflow the number of subsystem changes for
    // each subsystem we touch.
    ASSERT_LT(sub_set_->set[i].sched.xen_schedules,
              kMaxExpectedSchedulesPerSubsystem);
  }
}

TEST_F(SchedTest, HypervisorSubsystemWCTLessThanSubsystemWCT)
{
  for (int i = 0; i < sub_set_->set_size; i++) {
    // Cannot spend longer in another VM than spent executing a subsystem.
    ASSERT_EQ(-1, rscfl_timespec_compare(&sub_set_->set[i].sched.wct_out_hyp,
                                         &sub_set_->set[i].cpu.wall_clock_time));
  }
}

TEST_F(SchedTest, EvenNumberOfHypervisorSchedulingEvents)
{
  for (int i = 0; i < sub_set_->set_size; i++) {
    // Whenever we take a vmexit in a subsystem we need to also get a vmentry
    // before finishing the subsystem. Therefore the number of scheduling events
    // must be divisible by two.
    ASSERT_EQ(0, sub_set_->set[i].sched.xen_schedules % 2);
  }
}

TEST_F(SchedTest, CyclesSpentInHypervisorDoesntOverflow)
{
  for (int i = 0; i < sub_set_->set_size; i++) {
    // The number of cycles spent in other domains shouldn't overflow.
    ASSERT_LT(sub_set_->set[i].sched.xen_sched_cycles,
              kMaxExpectedCyclesSpentInHypervisorPerSubsystem);
  }
}

TEST_F(SchedTest, HypervisorCreditMaxBiggerThanMin)
{
  int min_credit;
  int max_credit;
  for (int i = 0; i < sub_set_->set_size; i++) {
    min_credit = sub_set_->set[i].sched.xen_credits_min;
    max_credit = sub_set_->set[i].sched.xen_credits_max;
    // Uninitialised value for min_credit is INT_MAX
    if (min_credit != INT_MAX) {
      // Ensure min < max if they're intialised.
      ASSERT_LE(min_credit, max_credit);
    }
  }
}

TEST_F(SchedTest, HypervisorCreditBothSetOrBothUnset)
{
  int min_credit;
  int max_credit;
  for (int i = 0; i < sub_set_->set_size; i++) {
    min_credit = sub_set_->set[i].sched.xen_credits_min;
    max_credit = sub_set_->set[i].sched.xen_credits_max;
    // Uninitialised value for min_credit is INT_MAX
    if (min_credit == INT_MAX) {
      // min_credit is not initialised, so max credit shouldn't be.
      ASSERT_EQ(INT_MIN, max_credit);
    } else {
      // min_credit is initialised, so max credit should be.
      ASSERT_NE(INT_MIN, max_credit);
    }
  }
}

TEST_F(SchedTest, HypervisorCreditSetIfSubsysScheduledOut)
{
  for (int i = 0; i < sub_set_->set_size; i++) {
    if (sub_set_->set[i].sched.xen_schedules) {
      // We have taken a vm-exit in this subsystem, so should have a new min/max
      // credit.
      ASSERT_LT(sub_set_->set[i].sched.xen_credits_min, INT_MAX);
      ASSERT_GT(sub_set_->set[i].sched.xen_credits_max, INT_MIN);
    }
  }
}

/*
 * All blocks cause a schedule a block, so we should have fewer blocks than
 * schedules.
 */
TEST_F(SchedTest, HypervisorBlocksLessThanSchedules)
{
  for (int i = 0; i < sub_set_->set_size; i++) {
    ASSERT_LE(sub_set_->set[i].sched.xen_blocks,
              sub_set_->set[i].sched.xen_schedules / 2);
  }
}

/*
 * All yields cause a schedule a block, so we should have fewer or eq yields
 * than schedules.
 */
TEST_F(SchedTest, HypervisorYieldsLessThanSchedules)
{
  for (int i = 0; i < sub_set_->set_size; i++) {
    // Every yield causes a schedule out + a schedule in. Therefore we should
    ASSERT_LE(sub_set_->set[i].sched.xen_yields,
              sub_set_->set[i].sched.xen_schedules / 2);
  }
}

TEST_F(SchedTest, HypervisorBlocksPlusYieldsLessThanSchedules)
{
  for (int i = 0; i < sub_set_->set_size; i++) {
    // Every yield and every block causes a schedule out + in, therefore we
    // should have fewer yields + blocks than 1/2 of the schedules.
    ASSERT_LE(
        sub_set_->set[i].sched.xen_yields + sub_set_->set[i].sched.xen_blocks,
        sub_set_->set[i].sched.xen_schedules / 2);
  }
}
