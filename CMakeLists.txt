cmake_minimum_required(VERSION 3.24)

project(lcpush VERSION 0.2.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

option(LCPUSH_BUILD_TESTS "Build the lcpush test suite" ON)
option(LCPUSH_STRICT "Treat compiler warnings as errors" OFF)
option(LCPUSH_SANITIZERS "Enable address and undefined behavior sanitizers" OFF)

find_package(CURL REQUIRED)

include(cmake/deps.cmake)

add_subdirectory(src/lcpush)

set(LCPUSH_TARGETS lcpush_core lcpush)

if(LCPUSH_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
  list(APPEND LCPUSH_TARGETS lcpush_tests)
endif()

if(LCPUSH_STRICT)
  if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    foreach(target IN LISTS LCPUSH_TARGETS)
      target_compile_options(${target} PRIVATE
        -Wall -Wextra -Wpedantic -Wconversion -Werror
      )
    endforeach()
  elseif(MSVC)
    foreach(target IN LISTS LCPUSH_TARGETS)
      target_compile_options(${target} PRIVATE /W4 /WX)
    endforeach()
  endif()
endif()

if(LCPUSH_SANITIZERS)
  if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    message(FATAL_ERROR "LCPUSH_SANITIZERS requires Clang or GCC")
  endif()
  foreach(target IN LISTS LCPUSH_TARGETS)
    target_compile_options(${target} PRIVATE
      -fsanitize=address,undefined -fno-omit-frame-pointer
    )
  endforeach()
  target_link_options(lcpush PRIVATE -fsanitize=address,undefined)
  if(LCPUSH_BUILD_TESTS)
    target_link_options(lcpush_tests PRIVATE -fsanitize=address,undefined)
  endif()
endif()
