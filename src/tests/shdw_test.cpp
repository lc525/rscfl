#include "gtest/gtest.h"

#include <iostream>
#include <fstream>

#include <rscfl/costs.h>
#include <rscfl/user/res_api.h>

class ShdwTest : public testing::Test
{
 protected:
  void SetUp()
  {
    rhdl_ = rscfl_init();
    ASSERT_NE(nullptr, rhdl_);
  }

  rscfl_handle rhdl_;
  const int kNumShdwSwitches {1935};
  const int kNumRepeats {5};
  const int kNumRepeatsCreateShdw {10};
};

static struct timespec wct_test_get_time(void)
{
  struct timespec ts;
  // We were originally using CLOCK_PROCESS_CPUTIME_ID but were occasionally
  // seeing strange (very small) values. By using CLOCK_MONOTONIC_RAW we're reading
  // a clock more similar to that of the kernel.
  clock_gettime(CLOCK_MONOTONIC_RAW, &ts);
  return ts;
}

TEST_F (ShdwTest, CanCreateAShdwKernelWithoutCrashing)
{
  struct accounting acct;
  shdw_hdl shdw;
  ASSERT_EQ(0, rscfl_spawn_shdw(rhdl_, &shdw));
  ASSERT_GT(shdw, 0);
}

TEST_F(ShdwTest, CanSwitchToAShdwKernelWithoutCrashing)
{
  struct accounting acct;
  // Switch to shadow 1 for every page.
  ASSERT_EQ(0, rscfl_use_shdw_pages(rhdl_, 1, 0));
}

TEST_F(ShdwTest, CostOfCreatingAShdwKernel)
{
  struct accounting acct;
  std::ofstream ofs;
  shdw_hdl shdw;
  ofs.open("create_shdw_test_performance.csv");

  for (int i = 0; i < kNumRepeatsCreateShdw; i++) {
    struct timespec val_pre = wct_test_get_time();
    ASSERT_EQ(0, rscfl_spawn_shdw(rhdl_, &shdw));
    ASSERT_GT(shdw, 0);
    struct timespec val_post = wct_test_get_time();
    rscfl_timespec_diff(&val_post, &val_pre);
    ofs << i << "," << val_post.tv_nsec / 1000 << std::endl;
  }
  ofs.close();
}

/*
 * Now look at the overheads of swapping shadow kernel each time.
 */
TEST_F(ShdwTest, RepeatedlySwitchShadowWithoutCrashing)
{
  struct accounting acct;
  struct subsys_idx_set *aggr = rscfl_get_new_aggregator(7);
  std::ofstream ofs;
  ofs.open("shdw_test_performance.csv");
  for (int j = 0; j < kNumRepeats; j++){
    for (int i = 1; i < kNumShdwSwitches; i+=9) {
      struct timespec val_pre = wct_test_get_time();
      ASSERT_EQ(0, rscfl_use_shdw_pages(rhdl_, 1, i));
      struct timespec val_post = wct_test_get_time();

      rscfl_timespec_diff(&val_post, &val_pre);
      ofs << i << "," << val_post.tv_nsec / 1000 << std::endl;
    }
  }
  ofs.close();
}
