// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Roman Parpalak and Denis Utkin

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include "exact_key.hpp"
#include "omatrix.hpp"

#include <vector>

namespace {

std::vector<size_t> n5_gens() {
    return {1, 3, 2, 1, 3, 0, 1, 2, 1, 3};
}

std::vector<size_t> n9_gens() {
    return {1, 3, 5, 7, 6, 5, 4, 3, 2, 1, 0, 1, 3, 5, 4, 3, 2,
            1, 3, 5, 7, 6, 5, 4, 3, 2, 1, 3, 5, 7, 6, 5, 4, 3, 5, 7};
}

} // namespace

TEST_CASE("compact_key returns value for full-layout n=5") {
    auto o = get_OMatrix(n5_gens());
    REQUIRE(o.has_value());

    auto key = compact_key(*o, 5);
    REQUIRE(key.has_value());
    CHECK(key->n == 5);
    CHECK(key->bytes.size() == 5 * 4);
}

TEST_CASE("compact_key returns value for full-layout n=9") {
    auto o = get_OMatrix(n9_gens());
    REQUIRE(o.has_value());

    auto key = compact_key(*o, 9);
    REQUIRE(key.has_value());
    CHECK(key->n == 9);
    CHECK(key->bytes.size() == 9 * 8);
}

TEST_CASE("exact_min_o_key invariant under rotations on n=5") {
    auto o = get_OMatrix(n5_gens());
    REQUIRE(o.has_value());

    auto base = exact_min_o_key(*o, 5);
    REQUIRE(base.has_value());

    for (size_t i = 0; i < 2 * o->n(); ++i) {
        auto r = o->rotate(i);
        if (!r.has_value())
            continue;
        auto k = exact_min_o_key(*r, 5);
        REQUIRE(k.has_value());
        CHECK(k->key.bytes == base->key.bytes);
    }
}

TEST_CASE("exact_min_o_key invariant under rotations on n=9") {
    auto o = get_OMatrix(n9_gens());
    REQUIRE(o.has_value());

    auto base = exact_min_o_key(*o, 9);
    REQUIRE(base.has_value());

    for (size_t i = 0; i < 2 * o->n(); ++i) {
        auto r = o->rotate(i);
        if (!r.has_value())
            continue;
        auto k = exact_min_o_key(*r, 9);
        REQUIRE(k.has_value());
        CHECK(k->key.bytes == base->key.bytes);
    }
}

TEST_CASE("get_projective_orbit_keys produces well-formed orbit on n=5") {
    auto o = get_OMatrix(n5_gens());
    REQUIRE(o.has_value());

    auto orbit = get_projective_orbit_keys(*o, 5);
    REQUIRE(orbit.has_value());
    CHECK(orbit->e_keys.size() >= 1);
    for (const auto& k : orbit->e_keys) {
        CHECK(k.n == 5);
        CHECK(k.bytes.size() == 5 * 4);
    }
    CHECK(orbit->p_key.n == 5);
    CHECK(orbit->p_key.bytes.size() == 5 * 4);
}

TEST_CASE("get_projective_orbit_keys produces well-formed orbit on n=9") {
    auto o = get_OMatrix(n9_gens());
    REQUIRE(o.has_value());

    auto orbit = get_projective_orbit_keys(*o, 9);
    REQUIRE(orbit.has_value());
    CHECK(orbit->e_keys.size() >= 1);
    for (const auto& k : orbit->e_keys) {
        CHECK(k.n == 9);
        CHECK(k.bytes.size() == 9 * 8);
    }
    CHECK(orbit->p_key.n == 9);
    CHECK(orbit->p_key.bytes.size() == 9 * 8);
}
