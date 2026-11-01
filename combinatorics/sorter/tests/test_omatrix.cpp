// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Roman Parpalak and Denis Utkin

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include "omatrix.hpp"

#include <vector>

TEST_CASE("get_OMatrix round-trip n=5") {
    std::vector<size_t> gens = {1, 3, 2, 1, 3, 0, 1, 2, 1, 3};
    auto o = get_OMatrix(gens);
    REQUIRE(o.has_value());
    CHECK(o->n() == 5);
    CHECK(o->get_num_vertices() == gens.size());

    auto regenerated = o->get_generators();
    auto o2 = get_OMatrix(regenerated);
    REQUIRE(o2.has_value());
    CHECK(*o2 == *o);
}

TEST_CASE("get_OMatrix round-trip n=9") {
    std::vector<size_t> gens = {1, 3, 5, 7, 6, 5, 4, 3, 2, 1, 0, 1, 3, 5, 4, 3, 2,
                                1, 3, 5, 7, 6, 5, 4, 3, 2, 1, 3, 5, 7, 6, 5, 4, 3, 5, 7};
    auto o = get_OMatrix(gens);
    REQUIRE(o.has_value());
    CHECK(o->n() == 9);
    CHECK(o->get_num_vertices() == gens.size());

    auto regenerated = o->get_generators();
    auto o2 = get_OMatrix(regenerated);
    REQUIRE(o2.has_value());
    CHECK(*o2 == *o);
}

TEST_CASE("min_o is idempotent on n=5") {
    std::vector<size_t> gens = {1, 3, 2, 1, 3, 0, 1, 2, 1, 3};
    auto o = get_OMatrix(gens);
    REQUIRE(o.has_value());

    OMatrix m1 = o->min_o();
    OMatrix m2 = m1.min_o();
    CHECK(m1 == m2);
}

TEST_CASE("min_o is idempotent on n=9") {
    std::vector<size_t> gens = {1, 3, 5, 7, 6, 5, 4, 3, 2, 1, 0, 1, 3, 5, 4, 3, 2,
                                1, 3, 5, 7, 6, 5, 4, 3, 2, 1, 3, 5, 7, 6, 5, 4, 3, 5, 7};
    auto o = get_OMatrix(gens);
    REQUIRE(o.has_value());

    OMatrix m1 = o->min_o();
    OMatrix m2 = m1.min_o();
    CHECK(m1 == m2);
}

TEST_CASE("mirror is involutive on n=5") {
    std::vector<size_t> gens = {1, 3, 2, 1, 3, 0, 1, 2, 1, 3};
    auto o = get_OMatrix(gens);
    REQUIRE(o.has_value());

    OMatrix mm = o->mirror().mirror();
    CHECK(mm == *o);
}

TEST_CASE("mirror is involutive on n=9") {
    std::vector<size_t> gens = {1, 3, 5, 7, 6, 5, 4, 3, 2, 1, 0, 1, 3, 5, 4, 3, 2,
                                1, 3, 5, 7, 6, 5, 4, 3, 2, 1, 3, 5, 7, 6, 5, 4, 3, 5, 7};
    auto o = get_OMatrix(gens);
    REQUIRE(o.has_value());

    OMatrix mm = o->mirror().mirror();
    CHECK(mm == *o);
}
