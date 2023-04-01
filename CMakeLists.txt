cmake_minimum_required(VERSION 3.12)

find_program(CMAKE_C_COMPILER NAMES $ENV{CC} gcc PATHS ENV PATH NO_DEFAULT_PATH)
find_program(CMAKE_CXX_COMPILER NAMES $ENV{CXX} g++ PATHS ENV PATH NO_DEFAULT_PATH)

project(blitz)

set(CMAKE_THREAD_LIBS_INIT "-lpthread" "-luring")
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_INSTALL_PREFIX "${PROJECT_SOURCE_DIR}/install")

SET(CMAKE_BUILD_TYPE "Debug")
# SET(CMAKE_BUILD_TYPE "Release")
SET(CMAKE_CXX_FLAGS_DEBUG "$ENV{CXXFLAGS} -O0 -Wall -g -ggdb -fPIC -no-pie")
SET(CMAKE_CXX_FLAGS_RELEASE "$ENV{CXXFLAGS} -O3 -Wall -fPIC -no-pie")

include_directories("core/inc")
file(GLOB CORE_SRC "core/src/*.cc")
add_library(blitz STATIC ${CORE_SRC})
install(TARGETS blitz DESTINATION "lib")
install(DIRECTORY "core/inc/" DESTINATION "include")

# add_subdirectory("example")
add_subdirectory("test")