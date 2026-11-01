# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Roman Parpalak and Denis Utkin

# The representation of an arrangement: reduced words, O-matrices, the
# projective closure, the face collapse, the exact identifiers. A library with
# no binaries; the C++ namespace is `addline`, where the files came from.
cmake_minimum_required(VERSION 3.16)
project(combinatorics_core CXX)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(CheckCXXCompilerFlag)
check_cxx_compiler_flag("-march=native" COMBINATORICS_CORE_HAS_MARCH_NATIVE)
if(COMBINATORICS_CORE_HAS_MARCH_NATIVE)
    add_compile_options($<$<CONFIG:Release>:-march=native>)
endif()

add_library(combinatorics_core
    src/word.cpp
    src/arrangement.cpp
    src/closure.cpp
    src/counts.cpp
    src/collapse.cpp
    src/omatrix.cpp
    src/identifiers.cpp
)

target_include_directories(combinatorics_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)

# Fixtures are shared with the tests of addline.
set(COMBINATORICS_TESTDATA ${CMAKE_CURRENT_SOURCE_DIR}/../testdata)
set(COMBINATORICS_REPO_DATA ${CMAKE_CURRENT_SOURCE_DIR}/../../data)

# Tests belong to this tree only.  Projects that pull this directory in for the
# library alone (EXCLUDE_FROM_ALL) would otherwise leave ctest with a case they
# never build.
if(PROJECT_IS_TOP_LEVEL)

enable_testing()

add_executable(core_tests
    tests/test_geometry.cpp
    tests/test_collapse.cpp
    tests/test_omatrix.cpp
    tests/test_identifiers.cpp
)
target_include_directories(core_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../third_party/doctest
)
target_compile_definitions(core_tests PRIVATE
    COMBINATORICS_DATA_DIR="${COMBINATORICS_TESTDATA}"
    REPO_DATA_DIR="${COMBINATORICS_REPO_DATA}"
)
target_link_libraries(core_tests PRIVATE combinatorics_core)
add_test(NAME core_tests COMMAND core_tests)

endif()
