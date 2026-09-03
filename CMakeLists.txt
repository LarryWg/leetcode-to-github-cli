cmake_minimum_required(VERSION 3.24)

project(lcpush VERSION 0.2.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

option(LCPUSH_BUILD_TESTS "Build the lcpush test suite" ON)

find_package(CURL REQUIRED)

include(cmake/deps.cmake)

add_subdirectory(src/lcpush)

if(LCPUSH_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()
