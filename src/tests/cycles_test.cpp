/**** Notice
 * cycles_test.cpp: rscfl source code
 *
 * Copyright 2015-2017 The rscfl owners <lucian.carata@cl.cam.ac.uk>
 *
 * This file is part of the rscfl open-source project: github.com/lc525/rscfl;
 * Its licensing is governed by the LICENSE file at the root of the project.
 **/

#include "gtest/gtest.h"

#include <errno.h>
#include <fcntl.h>
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
    ASSERT_EQ(0, rscfl_acct(rhdl_));

    ru64 val_pre = test_get_cycles();
    sockfd_ = socket(AF_LOCAL, SOCK_RAW, 0);
    ru64 val_post = test_get_cycles();
    user_cycles_ = val_post - val_pre;

    struct accounting acct_;
    ASSERT_EQ(0, rscfl_read_acct(rhdl_, &acct_));

    kernel_cycles_ = 0;
    int reduce_err = 0;
    // select cpu.cycles from all subsystems of a given acct and reduce
    // them to one value (their sum)
    reduce_err = REDUCE_SUBSYS(rint, rhdl_, &acct_, 1, &kernel_cycles_,
      [](subsys_accounting *s, rscfl_subsys id){ return &s->cpu.cycles; },
      [](ru64 *acct, const ru64* elem){ *acct += *elem; });

    ASSERT_EQ(0, reduce_err);
    ASSERT_NE(0, kernel_cycles_);
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
  std::cout << "\033[0;32m[          ] \033[mRSCFL explained "
            << percent_explained << "% of cycles seen in user space\n";
}
