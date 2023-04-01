cmake_minimum_required(VERSION 3.12)
project(buffer_test)

add_executable(buffer_test "main.cc")
target_link_libraries(buffer_test PRIVATE "gtest" "gtest_main" "pthread" "blitz")
