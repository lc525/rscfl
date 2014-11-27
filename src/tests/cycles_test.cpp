#include <errno.h>
#include <fcntl.h>
#include "gtest/gtest.h"
#include <stdio.h>
#include <sys/sendfile.h>
#include <sys/socket.h>

#include <rscfl/costs.h>
#include <rscfl/subsys_list.h>
#include <rscfl/user/res_api.h>

class CyclesTest : public testing::Test
{
 protected:
  static __inline__ ru64 test_get_cycles(void)
  {
    unsigned int hi, lo;
    __asm__ volatile("rdtsc" : "=a"(lo), "=d"(hi));
    return ((ru64)hi << 32) | lo;
  }

  virtual void SetUp()
  {
    rhdl_ = rscfl_init();
    ASSERT_NE(nullptr, rhdl_);
    ASSERT_EQ(0, rscfl_acct_next(rhdl_));

    ru64 val_pre = test_get_cycles();
    int sockfd_ = socket(AF_LOCAL, SOCK_RAW, 0);
    ru64 val_post = test_get_cycles();
    user_cycles_ = val_post - val_pre;

    struct accounting acct_;
    ASSERT_EQ(0, rscfl_read_acct(rhdl_, &acct_));

    // Now add all of the subsystem cycles
    kernel_cycles_ = 0;
    struct subsys_accounting *subsys;
    rscfl_subsys curr_sub;
    for (int i = 0; i < NUM_SUBSYSTEMS; i++) {
      curr_sub = (rscfl_subsys)i;
      if ((subsys = rscfl_get_subsys_by_id(rhdl_, &acct_, curr_sub)) != NULL) {
        kernel_cycles_ += subsys->cpu.cycles;
      }
    }
  }

  virtual void TearDown()
  {
    close(sockfd_);
  }

  rscfl_handle rhdl_;
  int sockfd_;
  ru64 user_cycles_;
  ru64 kernel_cycles_;
};

/*
 * Tests whether the number of cycles seen in the kernel is less than that seen
 * in user space. when opening a socket.
 */
TEST_F(CyclesTest, TestSocketCyclesMeasuredInKernelAreLessThanInUserspace)
{
  EXPECT_LT(kernel_cycles_, user_cycles_);
}

TEST_F(CyclesTest,
       SocketCyclesMeasuredByRscflAccountForMostOfThoseMesauredByUserspace)
{
  double percent_explained = (kernel_cycles_ / (double)user_cycles_) * 100;

  // Ensure that resourceful's measurements account for the expected number
  // of cycles, as measured from userspace to ensure we are accounting
  // correctly.
  EXPECT_GT(percent_explained, 40) << "Only explained " << percent_explained
                                   << "%\n";
}
