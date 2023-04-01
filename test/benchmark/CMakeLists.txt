cmake_minimum_required(VERSION 3.12)
project(benchmark)

add_executable(benchmark "main.cc")
target_link_libraries(benchmark PRIVATE "blitz" "gtest" "gtest_main" "pthread" "uring")
