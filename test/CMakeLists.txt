cmake_minimum_required(VERSION 3.12)
project(test)

find_package(GTest REQUIRED)
find_package(Threads REQUIRED)
include_directories("../install/include")
link_directories("../install/lib")
include_directories(${GTEST_INCLUDE_DIRS})
link_directories(${GTEST_LINK_DIR})

# add_subdirectory("buffer")
add_subdirectory("benchmark")
