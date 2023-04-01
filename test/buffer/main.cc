#include <gtest/gtest.h>
#include "buffer.h"

int add(int lhs, int rhs) { return lhs + rhs; }

int main(int argc, char const *argv[]) {

    EXPECT_EQ(add(1,1), 2);
    EXPECT_EQ(add(1,1), 1);

    return 0;
}
