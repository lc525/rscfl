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
  ASSERT_TRUE(rscfl_get_subsys_by_id(rhdl_, &acct_, NETWORKINGGENERAL) !=
              nullptr);
}

TEST_F(SocketTest, SocketHasCPUCyclesForNetworkingGeneral)
{
  struct subsys_accounting *subsys =
      rscfl_get_subsys_by_id(rhdl_, &acct_, NETWORKINGGENERAL);

  ASSERT_NE(subsys, nullptr);
  // Ensure we have a number of CPU cycles > 0 for NetworkingGeneral on opening
  // a socket.
  EXPECT_GT(subsys->cpu.cycles, 0);
}

// Tests for VFS subsystem, as socket create a file descriptor.

TEST_F(SocketTest, SocketTouchesVFS)
{
  // Socket must have touched networking general.
  ASSERT_TRUE(rscfl_get_subsys_by_id(
                  rhdl_, &acct_, FILESYSTEMSVFSANDINFRASTRUCTURE) != nullptr);
}

TEST_F(SocketTest, SocketHasCPUCyclesForVFS)
{
  struct subsys_accounting *subsys =
    rscfl_get_subsys_by_id(rhdl_, &acct_, FILESYSTEMSVFSANDINFRASTRUCTURE);

  ASSERT_NE(subsys, nullptr);
  // Ensure we have a number of CPU cycles > 0 for VFS on opening
  // a socket.
  EXPECT_GT(subsys->cpu.cycles, 0);
}

// Tests for security subsystem, as opening a socket requires security checks.

TEST_F(SocketTest, SocketTouchesSecuritySubsys)
{
  // Socket must have touched networking general.
  ASSERT_TRUE(rscfl_get_subsys_by_id(rhdl_, &acct_, SECURITYSUBSYSTEM) !=
              nullptr);
}

TEST_F(SocketTest, SocketHasCPUCyclesForSecuritySubsys)
{
  struct subsys_accounting *subsys =
      rscfl_get_subsys_by_id(rhdl_, &acct_, SECURITYSUBSYSTEM);
  ASSERT_NE(subsys, nullptr);
  // Ensure we have a number of CPU cycles > 0 for VFS on opening
  // a socket.
  ASSERT_GT(subsys->cpu.cycles, 0);
}

TEST_F(SocketTest, EveryNormalSubsystemEnteredHasAPositiveSubsysEntries)
{
  struct subsys_accounting *subsys;
  for (int i = 0; i < NUM_SUBSYSTEMS; i++) {
    subsys = rscfl_get_subsys_by_id(rhdl_, &acct_, (rscfl_subsys)i);
    if (subsys != nullptr) {
      // If a subsystem is not NULL then it has been called into. Therefore
      // it should not have 0 in its listed number of entries.
      if (i != USERSPACE_XEN) {
        ASSERT_LT(0, subsys->subsys_entries) << "No entries to subsys " << i;
      }
    }
  }
}

TEST_F(SocketTest, EveryNormalSubsystemEnteredHasAPositiveSubsysExits)
{
  struct subsys_accounting *subsys;
  for (int i = 0; i < NUM_SUBSYSTEMS; i++) {
    subsys = rscfl_get_subsys_by_id(rhdl_, &acct_, (rscfl_subsys)i);
    if (subsys != nullptr) {
      // If a subsystem is not NULL then it has been called into. Therefore
      // it should not have 0 in its listed number of exits.
      if (i != USERSPACE_XEN) {
        ASSERT_LT(0, subsys->subsys_exits) << "No exits from subsys " << i;
      }
    }
  }
}

TEST_F(SocketTest, XenUserspaceSubsystemEnteredHasNoSubsysExits)
{
  struct subsys_accounting *subsys;
  subsys = rscfl_get_subsys_by_id(rhdl_, &acct_, USERSPACE_XEN);
  if (subsys != nullptr) {
    // We do not want exits from the xen userspace subsystem.
    ASSERT_EQ(0, subsys->subsys_exits)
        << "Userspace xen subsys shouldn't have exits.";
  }
}

TEST_F(SocketTest, XenUserspaceSubsystemEnteredHasNoSubsysEntries)
{
  struct subsys_accounting *subsys;
  subsys = rscfl_get_subsys_by_id(rhdl_, &acct_, USERSPACE_XEN);
  if (subsys != nullptr) {
    // We do not want entries into the xen userspace subsystem.
    ASSERT_EQ(0, subsys->subsys_entries)
        << "Userspace xen subsys shouldn't have entries.";
  }
}

TEST_F(SocketTest, TotalSubsysEntriesEqualsTotalSubsysExits)
{
  struct subsys_accounting *subsys;
  ru64 subsys_entries{0};
  ru64 subsys_exits{0};

  int reduce_err;

  reduce_err = REDUCE_SUBSYS(
      rint, rhdl_, &acct_, 0, &subsys_entries,
      [](subsys_accounting *s, rscfl_subsys id) { return &s->subsys_entries; },
      [](ru64 *x, const ru64 *y) { *x += *y; });
  // Ensure reduce hasn't failed.
  EXPECT_EQ(0, reduce_err);

  reduce_err = REDUCE_SUBSYS(
      rint, rhdl_, &acct_, 0, &subsys_exits,
      [](subsys_accounting *s, rscfl_subsys id) { return &s->subsys_exits; },
      [](ru64 *x, const ru64 *y) { *x += *y; });
  // Ensure second reduce hasn't failed.
  EXPECT_EQ(0, reduce_err);

  // Whenever we enter a subsystem we also leave it. Therefore these two values
  // should be identical. If they're not, we're misaccounting.
  ASSERT_EQ(subsys_entries, subsys_exits);
}

// Misc. tests.

// If we open 3 sockets, they shouldn't all take the same number of cycles
// to open.
TEST_F(SocketTest, RscflGivesDifferentResultsForRepeatedSocketOpens)
{
  auto* subsys_acct = rscfl_get_subsys_by_id(rhdl_, &acct_, SECURITYSUBSYSTEM);
  ASSERT_NE(subsys_acct, nullptr);
  int cycles0 = subsys_acct->cpu.cycles;

  ASSERT_EQ(0, rscfl_acct_next(rhdl_));
  sockfd_ = socket(AF_LOCAL, SOCK_RAW, 0);
  // If we can't initialise a socket, something has gone wrong.
  EXPECT_GT(sockfd_, 0);
  // We must be able to read our struct accounting back from rscfl.
  ASSERT_EQ(0, rscfl_read_acct(rhdl_, &acct_));

  subsys_acct = rscfl_get_subsys_by_id(rhdl_, &acct_, SECURITYSUBSYSTEM);
  ASSERT_NE(subsys_acct, nullptr);
  int cycles1 = subsys_acct->cpu.cycles;

  ASSERT_EQ(0, rscfl_acct_next(rhdl_));
  sockfd_ = socket(AF_LOCAL, SOCK_RAW, 0);
  // If we can't initialise a socket, something has gone wrong.
  EXPECT_GT(sockfd_, 0);
  // We must be able to read our struct accounting back from rscfl.
  ASSERT_EQ(0, rscfl_read_acct(rhdl_, &acct_));

  subsys_acct = rscfl_get_subsys_by_id(rhdl_, &acct_, SECURITYSUBSYSTEM);
  ASSERT_NE(subsys_acct, nullptr);
  int cycles2 = subsys_acct->cpu.cycles;

  ASSERT_FALSE((cycles0 == cycles1) && (cycles1 == cycles2));
}
