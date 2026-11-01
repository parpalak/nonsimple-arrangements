# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Roman Parpalak and Denis Utkin

cmake_minimum_required(VERSION 3.26)
project(sorter)

# EXCLUDE_FROM_ALL: only the library is wanted, not the tests that come with
# it.  Sorter needs the representation -- words, O-matrices, closures, exact
# identifiers -- and nothing of addline, the collar search or the moves.
if(NOT TARGET combinatorics_core)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../core
                     ${CMAKE_CURRENT_BINARY_DIR}/core
                     EXCLUDE_FROM_ALL)
endif()

# doctest is shared with core and addline, so it lives at the root of the
# repository.  argparse stays here: nothing outside sorter uses it.
set(DOCTEST_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../third_party/doctest)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Aggressive optimization for Release (the default build type). These are
# additive on top of CMake's default Release flags (-O3 -DNDEBUG), so NDEBUG
# is preserved. -march=native ties the binary to the build host, which is what
# we want for local research runs and same-host CI. Applied to every target
# below, so they must be set before the add_library/add_executable calls.
include(CheckCXXCompilerFlag)
include(CheckIPOSupported)

check_cxx_compiler_flag("-march=native" SORTER_HAS_MARCH_NATIVE)
if(SORTER_HAS_MARCH_NATIVE)
    add_compile_options($<$<CONFIG:Release>:-march=native>)
endif()
check_cxx_compiler_flag("-funroll-loops" SORTER_HAS_FUNROLL)
if(SORTER_HAS_FUNROLL)
    add_compile_options($<$<CONFIG:Release>:-funroll-loops>)
endif()

check_ipo_supported(RESULT SORTER_IPO_OK OUTPUT SORTER_IPO_MSG)
if(SORTER_IPO_OK)
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE ON)
endif()

add_library(sorter_core
    generator_parser.cpp
    line_reader.cpp
    omatrix.cpp
    fixed_omatrix.cpp
    id_labels.cpp
    exact_key.cpp
    traced_transform.cpp
    line_permutation_group.cpp
    symmetry_analyzer.cpp
    json_writer.cpp
    progress.cpp
    state_io.cpp
    resume.cpp
    wire_print.cpp
    configuration.cpp
    print_config.cpp
    stream_drivers.cpp
)

target_include_directories(sorter_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(sorter_core PUBLIC combinatorics_core)

add_executable(sorter main.cpp)

target_include_directories(sorter PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/argparse
)

target_link_libraries(sorter PRIVATE sorter_core)

add_executable(sym_detect sym_detect.cpp)

target_include_directories(sym_detect PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/argparse
)

target_link_libraries(sym_detect PRIVATE sorter_core)

enable_testing()

add_executable(sorter_tests
    tests/test_generator_parser.cpp
    tests/test_multiple_points.cpp
)

target_include_directories(sorter_tests PRIVATE
    ${DOCTEST_DIR}
)

target_link_libraries(sorter_tests PRIVATE sorter_core)

add_test(NAME sorter_tests COMMAND sorter_tests)

add_executable(line_reader_tests
    tests/test_line_reader.cpp
)

target_include_directories(line_reader_tests PRIVATE
    ${DOCTEST_DIR}
)

target_link_libraries(line_reader_tests PRIVATE sorter_core)

find_package(Threads REQUIRED)
target_link_libraries(line_reader_tests PRIVATE Threads::Threads)

add_test(NAME line_reader_tests COMMAND line_reader_tests)

add_executable(test_omatrix
    tests/test_omatrix.cpp
)

target_include_directories(test_omatrix PRIVATE
    ${DOCTEST_DIR}
)

target_link_libraries(test_omatrix PRIVATE sorter_core)

add_test(NAME test_omatrix COMMAND test_omatrix)

add_executable(test_exact_key
    tests/test_exact_key.cpp
)

target_include_directories(test_exact_key PRIVATE
    ${DOCTEST_DIR}
)

target_link_libraries(test_exact_key PRIVATE sorter_core)

add_test(NAME test_exact_key COMMAND test_exact_key)

add_executable(test_id_labels
    tests/test_id_labels.cpp
)

target_include_directories(test_id_labels PRIVATE
    ${DOCTEST_DIR}
)

target_link_libraries(test_id_labels PRIVATE sorter_core)

add_test(NAME test_id_labels COMMAND test_id_labels)

add_executable(test_traced_transform
    tests/test_traced_transform.cpp
)

target_include_directories(test_traced_transform PRIVATE
    ${DOCTEST_DIR}
)

target_link_libraries(test_traced_transform PRIVATE sorter_core)

add_test(NAME test_traced_transform COMMAND test_traced_transform)

add_executable(test_line_permutation_group
    tests/test_line_permutation_group.cpp
)

target_include_directories(test_line_permutation_group PRIVATE
    ${DOCTEST_DIR}
)

target_link_libraries(test_line_permutation_group PRIVATE sorter_core)

add_test(NAME test_line_permutation_group COMMAND test_line_permutation_group)

add_executable(test_symmetry_analyzer
    tests/test_symmetry_analyzer.cpp
)

target_include_directories(test_symmetry_analyzer PRIVATE
    ${DOCTEST_DIR}
)

target_link_libraries(test_symmetry_analyzer PRIVATE sorter_core)

add_test(NAME test_symmetry_analyzer COMMAND test_symmetry_analyzer)

add_executable(test_json_writer
    tests/test_json_writer.cpp
)

target_include_directories(test_json_writer PRIVATE
    ${DOCTEST_DIR}
)

target_link_libraries(test_json_writer PRIVATE sorter_core)

add_test(NAME test_json_writer COMMAND test_json_writer)

add_executable(test_sym_detect_n17
    tests/test_sym_detect_n17.cpp
)

target_include_directories(test_sym_detect_n17 PRIVATE
    ${DOCTEST_DIR}
)

target_link_libraries(test_sym_detect_n17 PRIVATE sorter_core)

target_compile_definitions(test_sym_detect_n17 PRIVATE
    PSEUDOLINE_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests/data"
)

add_test(NAME test_sym_detect_n17 COMMAND test_sym_detect_n17)

add_executable(test_sym_detect_n21_golden
    tests/test_sym_detect_n21_golden.cpp
)

target_include_directories(test_sym_detect_n21_golden PRIVATE
    ${DOCTEST_DIR}
)

target_link_libraries(test_sym_detect_n21_golden PRIVATE sorter_core)

target_compile_definitions(test_sym_detect_n21_golden PRIVATE
    PSEUDOLINE_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests/data"
)

add_test(NAME test_sym_detect_n21_golden COMMAND test_sym_detect_n21_golden)

add_executable(test_fixed_omatrix
    tests/test_fixed_omatrix.cpp
)

target_include_directories(test_fixed_omatrix PRIVATE
    ${DOCTEST_DIR}
)

target_link_libraries(test_fixed_omatrix PRIVATE sorter_core)

add_test(NAME test_fixed_omatrix COMMAND test_fixed_omatrix)

add_executable(test_resume
    tests/test_resume.cpp
)

target_include_directories(test_resume PRIVATE
    ${DOCTEST_DIR}
)

target_link_libraries(test_resume PRIVATE sorter_core)

add_test(NAME test_resume COMMAND test_resume)
