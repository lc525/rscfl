#include <errno.h>
#include "gtest/gtest.h"
#include <stdio.h>
#include <sys/sendfile.h>
#include <fcntl.h>
#include <sys/socket.h>

#include <rscfl/costs.h>
#include <rscfl/user/res_api.h>

class CyclesTest : public testing::Test
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

static __inline__ ru64 test_get_cycles(void)
{
  unsigned int hi, lo;
  __asm__ volatile("rdtsc" : "=a" (lo), "=d" (hi));
  return ((ru64)hi << 32) | lo;
}

/*
 * Tests whether the number of cycles seen in the kernel is less than that seen
 * in user space. when opening a socket.
 */
TEST_F(CyclesTest, SocketCyclesValidation)
{
  ASSERT_EQ(0, rscfl_acct_next(rhdl_));

  ru64 val_pre = test_get_cycles();
  int sockfd_ = socket(AF_LOCAL, SOCK_RAW, 0);
  ru64 val_post = test_get_cycles();
  ru64 user_cycles = val_post - val_pre;

  struct accounting acct_;
  ASSERT_EQ(0, rscfl_read_acct(rhdl_, &acct_));

  // Now add all of the subsystem cycles
  ru64 cycles_net =
      get_subsys_accounting(rhdl_, &acct_, NETWORKINGGENERAL)->cpu.cycles;

  ru64 cycles_vfs =
      get_subsys_accounting(rhdl_, &acct_, FILESYSTEMSVFSANDINFRASTRUCTURE)
          ->cpu.cycles;

  ru64 cycles_security =
      get_subsys_accounting(rhdl_, &acct_, SECURITYSUBSYSTEM)->cpu.cycles;

  ru64 kernel_cycles = cycles_net + cycles_vfs + cycles_security;

  ASSERT_LT(kernel_cycles, user_cycles);
}

