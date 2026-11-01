# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Roman Parpalak and Denis Utkin

# Drawing one more line through a seed. The representation itself lives in
# ../core.
cmake_minimum_required(VERSION 3.16)
project(addline CXX)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(CheckCXXCompilerFlag)
check_cxx_compiler_flag("-march=native" ADDLINE_HAS_MARCH_NATIVE)
if(ADDLINE_HAS_MARCH_NATIVE)
    add_compile_options($<$<CONFIG:Release>:-march=native>)
endif()

if(NOT TARGET combinatorics_core)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../core core EXCLUDE_FROM_ALL)
endif()

add_library(addline_core
    src/route.cpp
    src/emit.cpp
)
target_include_directories(addline_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(addline_core PUBLIC combinatorics_core)

add_executable(addline src/main.cpp)
target_link_libraries(addline PRIVATE addline_core)

if(PROJECT_IS_TOP_LEVEL)

enable_testing()

add_executable(addline_tests
    tests/test_route.cpp
    tests/test_emit.cpp
)
target_include_directories(addline_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../third_party/doctest
)
target_compile_definitions(addline_tests PRIVATE
    COMBINATORICS_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/../testdata"
    REPO_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/../../data"
)
target_link_libraries(addline_tests PRIVATE addline_core)
add_test(NAME addline_tests COMMAND addline_tests)

endif()
