#include <errno.h>
#include "gtest/gtest.h"
#include <stdio.h>
#include <sys/socket.h>

#include <rscfl/costs.h>
#include <rscfl/user/res_api.h>

class SocketTest : public testing::Test
{

 protected:
  virtual void SetUp()
  {
    // We must be able to initialise resourceful.
    rhdl_ = rscfl_init();
    ASSERT_NE(nullptr, rhdl_);
    // We must be able to account next.
    ASSERT_EQ(0, rscfl_acct_next(rhdl_));
    sockfd_ = socket(AF_LOCAL, SOCK_RAW, 0);
    // If we can't initialise a socket, something has gone wrong.
    EXPECT_GT(sockfd_, 0);
    // We must be able to read our struct accounting back from rscfl.
    ASSERT_EQ(0, rscfl_read_acct(rhdl_, &acct_));
  }

  virtual void TearDown()
  {
    close(sockfd_);
    // TODO(oc243): Cleanup rhdl_, and acct_. Needs code in res_api.
  }

  rscfl_handle rhdl_;
  struct accounting acct_;
  int sockfd_;
};

// Tests for NETWORKINGGENERAL subsystem.

TEST_F(SocketTest, SocketTouchesNetworkingGeneral)
{
  // Socket must have touched networking general.
  ASSERT_TRUE(get_subsys_accounting(rhdl_, &acct_, NETWORKINGGENERAL) !=
              nullptr);
}

TEST_F(SocketTest, SocketHasCPUCyclesForNetworkingGeneral)
{
  struct subsys_accounting *subsys =
      get_subsys_accounting(rhdl_, &acct_, NETWORKINGGENERAL);

  // Ensure we have a number of CPU cycles > 0 for NetworkingGeneral on opening
  // a socket.
  ASSERT_GT(subsys->cpu.cycles, 0);
}

// Tests for VFS subsystem, as socket create a file descriptor.

TEST_F(SocketTest, SocketTouchesVFS)
{
  // Socket must have touched networking general.
  ASSERT_TRUE(get_subsys_accounting(
                  rhdl_, &acct_, FILESYSTEMSVFSANDINFRASTRUCTURE) != nullptr);
}

TEST_F(SocketTest, SocketHasCPUCyclesForVFS)
{
  struct subsys_accounting *subsys =
    get_subsys_accounting(rhdl_, &acct_, FILESYSTEMSVFSANDINFRASTRUCTURE);

  // Ensure we have a number of CPU cycles > 0 for VFS on opening
  // a socket.
  ASSERT_GT(subsys->cpu.cycles, 0);
}

// Tests for security subsystem, as opening a socket requires security checks.

TEST_F(SocketTest, SocketTouchesSecuritySubsys)
{
  // Socket must have touched networking general.
  ASSERT_TRUE(get_subsys_accounting(
		rhdl_, &acct_, SECURITYSUBSYSTEM) != nullptr);
}

TEST_F(SocketTest, SocketHasCPUCyclesForSecuritySubsys)
{
  struct subsys_accounting *subsys =
    get_subsys_accounting(rhdl_, &acct_, SECURITYSUBSYSTEM);

  // Ensure we have a number of CPU cycles > 0 for VFS on opening
  // a socket.
  ASSERT_GT(subsys->cpu.cycles, 0);
}

// Misc. tests.

// If we open 3 sockets, they shouldn't all take the same number of cycles
// to open.
TEST_F(SocketTest, RscflGivesDifferentResultsForRepeatedSocketOpens)
{
  int cycles0 =
      get_subsys_accounting(rhdl_, &acct_, SECURITYSUBSYSTEM)->cpu.cycles;

  ASSERT_EQ(0, rscfl_acct_next(rhdl_));
  sockfd_ = socket(AF_LOCAL, SOCK_RAW, 0);
  // If we can't initialise a socket, something has gone wrong.
  EXPECT_GT(sockfd_, 0);
  // We must be able to read our struct accounting back from rscfl.
  ASSERT_EQ(0, rscfl_read_acct(rhdl_, &acct_));
  int cycles1 =
      get_subsys_accounting(rhdl_, &acct_, SECURITYSUBSYSTEM)->cpu.cycles;

  ASSERT_EQ(0, rscfl_acct_next(rhdl_));
  sockfd_ = socket(AF_LOCAL, SOCK_RAW, 0);
  // If we can't initialise a socket, something has gone wrong.
  EXPECT_GT(sockfd_, 0);
  // We must be able to read our struct accounting back from rscfl.
  ASSERT_EQ(0, rscfl_read_acct(rhdl_, &acct_));
  int cycles2 =
      get_subsys_accounting(rhdl_, &acct_, SECURITYSUBSYSTEM)->cpu.cycles;

  ASSERT_FALSE((cycles0 == cycles1) && (cycles1 == cycles2));
}
