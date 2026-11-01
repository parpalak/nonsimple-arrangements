// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Roman Parpalak and Denis Utkin

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include "generator_parser.hpp"

#include <limits>
#include <string>
#include <vector>

TEST_CASE("plain generators with odd values are kept unchanged") {
    auto parsed = parse_generators("0 1 0");

    CHECK(parsed.ok);
    CHECK_FALSE(parsed.expanded_even_generators);
    CHECK(parsed.gens == std::vector<size_t>{0, 1, 0});
}

TEST_CASE("even-only generators are expanded") {
    auto parsed = parse_generators("2 0 2");

    CHECK(parsed.ok);
    CHECK(parsed.expanded_even_generators);
    CHECK(parsed.gens == std::vector<size_t>{1, 3, 2, 1, 3, 0, 1, 2, 1, 3});
}

TEST_CASE("prefixed even-only generators are expanded after closing parenthesis") {
    auto parsed = parse_generators("[40.749517s] A=125 i=6.132704e+09) 2 0 2");

    CHECK(parsed.ok);
    CHECK(parsed.expanded_even_generators);
    CHECK(parsed.gens == std::vector<size_t>{1, 3, 2, 1, 3, 0, 1, 2, 1, 3});
}

TEST_CASE("even generator zero only adds right odd neighbor") {
    auto parsed = parse_generators("0");

    CHECK(parsed.ok);
    CHECK(parsed.expanded_even_generators);
    CHECK(parsed.gens == std::vector<size_t>{1, 0, 1});
}

TEST_CASE("invalid token is rejected") {
    auto parsed = parse_generators("0 abc 1");

    CHECK_FALSE(parsed.ok);
    CHECK(parsed.reason == "invalid generator token: abc");
}

TEST_CASE("negative token is rejected") {
    auto parsed = parse_generators("-1 0");

    CHECK_FALSE(parsed.ok);
    CHECK(parsed.reason == "invalid generator token: -1");
}

TEST_CASE("empty payload is rejected") {
    auto parsed = parse_generators("[40.749517s] A=125)");

    CHECK_FALSE(parsed.ok);
    CHECK(parsed.reason == "empty or non-generator line");
}

TEST_CASE("too large even generator is rejected before n overflow") {
    auto parsed = parse_generators(std::to_string(std::numeric_limits<size_t>::max() - 1));

    CHECK_FALSE(parsed.ok);
    CHECK(parsed.reason == "generator value is too large");
}

TEST_CASE("starred generator is parsed as one triple point") {
    auto parsed = parse_generators("0*");

    REQUIRE(parsed.ok);
    CHECK(parsed.has_multiple_points);
    CHECK(parsed.n == 3);
    REQUIRE(parsed.word.size() == 1);
    const addline::Step triple{0, 3};
    CHECK(parsed.word[0] == triple);
    CHECK(parsed.gens == std::vector<size_t>{0, 1, 0});
}

TEST_CASE("repeated stars after metadata are one fourfold point") {
    auto parsed = parse_generators("#1 direct ) 0**");

    REQUIRE_MESSAGE(parsed.ok, parsed.reason);
    const addline::Word fourfold{{0, 4}};
    CHECK(parsed.word == fourfold);
    CHECK(parsed.n == 4);
    CHECK(parsed.gens == std::vector<size_t>{0, 1, 0, 2, 1, 0});
}
