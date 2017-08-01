/**** Notice
 * gtest_sanity_check.cpp: rscfl source code
 *
 * Copyright 2015-2017 The rscfl owners <lucian.carata@cl.cam.ac.uk>
 *
 * This file is part of the rscfl open-source project: github.com/lc525/rscfl;
 * Its licensing is governed by the LICENSE file at the root of the project.
 **/

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
