#include "IntSet.h"
#include "UnitTest.h"
#include <iostream>

TEST_CASE(test_insert)
{
    IntSet  sets;
    EXPECT_TRUE(sets.InsertValue(5));
    EXPECT_TRUE(sets.InsertValue(6));
    EXPECT_FALSE(sets.InsertValue(6));
    EXPECT_TRUE(sets.InsertValue(1));

    EXPECT_TRUE(sets[2] == 6);

    EXPECT_TRUE(sets.Exist(1));
}

TEST_CASE(test_erase)
{
    IntSet  sets;
    sets.InsertValue(5);
    sets.InsertValue(6);
    sets.InsertValue(1);

    EXPECT_TRUE(sets.EraseValue(1) == 1);
    EXPECT_FALSE(sets.Exist(1));

    EXPECT_FALSE(sets.EraseValue(1) == 1);

    EXPECT_TRUE(sets.Size() == 2);
}

