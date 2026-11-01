// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Roman Parpalak and Denis Utkin

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include "exact_key.hpp"
#include "fixed_omatrix.hpp"
#include "omatrix.hpp"

#include <cstdint>
#include <cstring>
#include <vector>

namespace {

std::vector<uint8_t> omatrix_to_flat(const OMatrix& o) {
    std::vector<uint8_t> bytes;
    bytes.reserve(o.n() * (o.n() - 1));
    for (const auto& line : o.intersections) {
        for (size_t v : line)
            bytes.push_back(static_cast<uint8_t>(v));
    }
    return bytes;
}

std::vector<size_t> n5_gens()  { return {1, 3, 2, 1, 3, 0, 1, 2, 1, 3}; }
std::vector<size_t> n9_gens()  { return {1, 3, 5, 7, 6, 5, 4, 3, 2, 1, 0, 1, 3, 5, 4, 3, 2,
                                          1, 3, 5, 7, 6, 5, 4, 3, 2, 1, 3, 5, 7, 6, 5, 4, 3, 5, 7}; }

void check_rotate_matches_omatrix(const OMatrix& base) {
    auto fixed = FixedOMatrix::from_omatrix(base);
    REQUIRE(fixed.has_value());

    const size_t n = base.n();
    std::vector<uint8_t> scratch(n * (n - 1));

    for (size_t r = 0; r < 2 * n; ++r) {
        auto rotated = base.rotate(r);
        if (!rotated.has_value())
            continue;  // closed configs: shouldn't happen, but be safe

        fixed_o::rotate(fixed->data(), n, r, scratch.data());
        std::vector<uint8_t> expected = omatrix_to_flat(*rotated);

        REQUIRE(scratch.size() == expected.size());
        CHECK(std::memcmp(scratch.data(), expected.data(), scratch.size()) == 0);

        // hash equivalence: hash_bytes(scratch) == rotated->hash()
        CHECK(hash_bytes(scratch.data(), scratch.size()) == rotated->hash());
    }
}

void check_mirror_matches_omatrix(const OMatrix& base) {
    auto fixed = FixedOMatrix::from_omatrix(base);
    REQUIRE(fixed.has_value());

    const size_t n = base.n();
    std::vector<uint8_t> scratch(n * (n - 1));

    OMatrix mirrored = base.mirror();
    fixed_o::mirror(fixed->data(), n, scratch.data());
    std::vector<uint8_t> expected = omatrix_to_flat(mirrored);

    REQUIRE(scratch.size() == expected.size());
    CHECK(std::memcmp(scratch.data(), expected.data(), scratch.size()) == 0);
    CHECK(hash_bytes(scratch.data(), scratch.size()) == mirrored.hash());
}

}  // namespace

TEST_CASE("FixedOMatrix::from_omatrix rejects non-closed") {
    OMatrix o;
    o.intersections = {{1, 2}, {0}, {0}};
    o.parallels = {{0, 2}, {2, 0}};
    auto r = FixedOMatrix::from_omatrix(o);
    CHECK_FALSE(r.has_value());
}

TEST_CASE("fixed_o::rotate matches OMatrix::rotate on n=5") {
    auto o = get_OMatrix(n5_gens());
    REQUIRE(o.has_value());
    check_rotate_matches_omatrix(*o);
}

TEST_CASE("fixed_o::mirror matches OMatrix::mirror on n=5") {
    auto o = get_OMatrix(n5_gens());
    REQUIRE(o.has_value());
    check_mirror_matches_omatrix(*o);
}

TEST_CASE("fixed_o::rotate matches OMatrix::rotate on n=9") {
    auto o = get_OMatrix(n9_gens());
    REQUIRE(o.has_value());
    check_rotate_matches_omatrix(*o);
}

TEST_CASE("fixed_o::mirror matches OMatrix::mirror on n=9") {
    auto o = get_OMatrix(n9_gens());
    REQUIRE(o.has_value());
    check_mirror_matches_omatrix(*o);
}

TEST_CASE("hash equivalence: hash_bytes(fixed) == OMatrix::hash()") {
    auto o = get_OMatrix(n5_gens());
    REQUIRE(o.has_value());
    auto fixed = FixedOMatrix::from_omatrix(*o);
    REQUIRE(fixed.has_value());
    CHECK(hash_bytes(fixed->data(), fixed->key_len()) == o->hash());
}

TEST_CASE("rotate then mirror matches mirror of rotate on n=5") {
    auto o = get_OMatrix(n5_gens());
    REQUIRE(o.has_value());
    auto fixed = FixedOMatrix::from_omatrix(*o);
    REQUIRE(fixed.has_value());

    const size_t n = o->n();
    std::vector<uint8_t> rot_buf(n * (n - 1));
    std::vector<uint8_t> rot_then_mirror(n * (n - 1));

    fixed_o::rotate(fixed->data(), n, 3, rot_buf.data());
    fixed_o::mirror(rot_buf.data(), n, rot_then_mirror.data());

    auto rotated = o->rotate(3);
    REQUIRE(rotated.has_value());
    auto rotated_mirrored = rotated->mirror();
    auto expected = omatrix_to_flat(rotated_mirrored);
    CHECK(std::memcmp(rot_then_mirror.data(), expected.data(), expected.size()) == 0);
}
