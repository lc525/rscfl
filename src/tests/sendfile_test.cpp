#include "gtest/gtest.h"

#include <fcntl.h>
#include <sys/sendfile.h>

#include <rscfl/costs.h>
#include <rscfl/user/res_api.h>

const int kBufSize = 1024;
const int kSuspectedOverflow = 999999999;

class SendFileTest : public testing::Test
{

 protected:
  virtual void SetUp()
  {

    // Initialise resourceful.
    rhdl_ = rscfl_init();
    ASSERT_NE(nullptr, rhdl_);

    int from_fd = open("/etc/hostname", O_RDONLY);
    EXPECT_LT(-1, from_fd);
    int to_fd = open("/tmp/rscfl_test", O_WRONLY | O_CREAT, 0644);
    EXPECT_LT(-1, to_fd);

    // Account for the call to sendfile.
    ASSERT_EQ(0, rscfl_acct_next(rhdl_));
    EXPECT_LT(0, sendfile(to_fd, from_fd, NULL, kBufSize));

    // We must be able to read our struct accounting back from rscfl.
    ASSERT_EQ(0, rscfl_read_acct(rhdl_, &acct_));
  }

  rscfl_handle rhdl_;
  struct accounting acct_;
};

// VFS tests.

TEST_F(SendFileTest, TestSendFileTouchesVFS)
{
  // We must be able to read our struct accounting back from rscfl.
  ASSERT_TRUE(rscfl_get_subsys_by_id(
              rhdl_, &acct_, FILESYSTEMSVFSANDINFRASTRUCTURE) != nullptr);
}

TEST_F(SendFileTest, TestSendFileHasCPUCyclesForVFS)
{
  struct subsys_accounting *subsys =
      rscfl_get_subsys_by_id(rhdl_, &acct_, FILESYSTEMSVFSANDINFRASTRUCTURE);
  ASSERT_NE(subsys, nullptr);
  // Ensure we have a number of CPU cycles > 0 for VFS on using sendfile.
  ASSERT_GT(subsys->cpu.cycles, 0);
}

TEST_F(SendFileTest, TestSendFileVFSCPUCyclesIsBelievable)
{
  struct subsys_accounting *subsys =
      rscfl_get_subsys_by_id(rhdl_, &acct_, FILESYSTEMSVFSANDINFRASTRUCTURE);
  ASSERT_NE(subsys, nullptr);
  // Ensure the CPU cycles don't look like an overflow has occurred.
  ASSERT_LT(subsys->cpu.cycles, kSuspectedOverflow);
}

/*
 * Ext4 tests.
 *
 * Testing ext4 exercsies a different code path to testing VFS in so far as
 * that ext4 is called using function pointers through a struct file_operations
 * rather than a callq instruction.
 *
 * These tests assume that /etc/hostname is mounted on an ext4 FS.
 */

TEST_F(SendFileTest, TestSendFileTouchesExt4)
{
  // We must be able to read our struct accounting back from rscfl.
  // Ext4 takes a differe
  ASSERT_TRUE(rscfl_get_subsys_by_id(
		rhdl_, &acct_, EXT4FILESYSTEM) != nullptr);
}

TEST_F(SendFileTest, TestSendFileExt4CPUCyclesIsBelievable)
{
  struct subsys_accounting *subsys =
    rscfl_get_subsys_by_id(rhdl_, &acct_, EXT4FILESYSTEM);

  // Ensure the CPU cycles don't look like an overflow has occurred.
  ASSERT_LT(subsys->cpu.cycles, kSuspectedOverflow);
}
