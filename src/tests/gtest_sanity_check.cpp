#include "gtest/gtest.h"

#include "rscfl/user/res_api.h"

TEST(GtestSanityCheck, AlwaysPASS)
{
    EXPECT_EQ(1, 1);
}

TEST(GtestSanityCheck, InitRscfl)
{
  ASSERT_TRUE(rscfl_init() != NULL) <<
    "Have you loaded (insmod/staprun) the rscfl kernel module?";
}
