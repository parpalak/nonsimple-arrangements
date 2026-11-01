// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Roman Parpalak and Denis Utkin

#include "doctest.h"

#include "identifiers.hpp"
#include "omatrix.hpp"
#include "word.hpp"

#include <set>
#include <vector>

using namespace addline;

namespace {

OMatrix matrix_of(const char* text, int n) {
    const ParseResult parsed = parse_word(text);
    REQUIRE_MESSAGE(parsed.ok, parsed.reason);
    const OMatrixResult matrix = omatrix_from_word(parsed.word, n);
    REQUIRE_MESSAGE(matrix.ok, matrix.reason);
    return matrix.o;
}

}  // namespace

TEST_CASE("simple identifiers are byte-compatible with sorter") {
    struct Golden { const char* word; int n; const char* did; const char* eid; const char* pid; };
    const Golden cases[] = {
        {"0 1 0", 3, "D3-1u0c5a5eszdz", "E3-1u0c5a5eszdz", "P3-1u0c5a5eszdz"},
        {"0 1 0 2 4 3 2 1 0 2 4 5 4 3 2 1 0 2 4 3 2", 7,
         "D7-1xi1hu47otg9n", "E7-1xi1hu47otg9n", "P7-1xi1hu47otg9n"},
        {"0 2 4 6 5 4 3 2 1 0 2 4 3 2 4 6 5 4 6 7 6 5 4 3 2 1 0 2 4 3 2 4 6 5 4 6", 9,
         "D9-fucx1svr4ckl", "E9-1xw8b9ugsk55", "P9-1xw8b9ugsk55"},
    };
    for (const Golden& golden : cases) {
        CAPTURE(golden.n);
        const OMatrix o = matrix_of(golden.word, golden.n);
        const ClassIdentifiers ids = class_identifiers(o, projective_closure(o));
        REQUIRE_MESSAGE(ids.ok, ids.reason);
        CHECK(ids.did == golden.did);
        CHECK(ids.eid == golden.eid);
        CHECK(ids.pid == golden.pid);
        CHECK(ids.d_key.bytes.size() ==
              static_cast<std::size_t>(golden.n * (golden.n - 1)));
    }
}

TEST_CASE("point boundaries distinguish adjacent triples from a fivefold point") {
    // Line 0 meets {1,2} and then {3,4} in two consecutive triple points.
    const OMatrix two = matrix_of("0* 2* 1 0 2 1", 5);
    const OMatrix five = matrix_of("0***", 5);
    ExactKey two_key, five_key;
    std::string reason;
    REQUIRE_MESSAGE(exact_d_key(two, two_key, reason), reason);
    REQUIRE_MESSAGE(exact_d_key(five, five_key, reason), reason);
    REQUIRE(two_key.bytes.size() == 24);
    REQUIRE(five_key.bytes.size() == 24);
    // 20 label bytes, ff 01, then the continuation mask.  Low row-0 bits:
    // [1,0,1] for [1,2]|[3,4], versus [1,1,1] for [1,2,3,4].
    CHECK((two_key.bytes[22] & 0x07) == 0x05);
    CHECK((five_key.bytes[22] & 0x07) == 0x07);
    CHECK(two_key.bytes != five_key.bytes);
}

TEST_CASE("a star changes the exact key and survives projective hashing") {
    const OMatrix starred = matrix_of("0*", 3);
    ExactKey d, e, p;
    std::string reason;
    REQUIRE_MESSAGE(exact_d_key(starred, d, reason), reason);
    REQUIRE_MESSAGE(exact_e_key(starred, e, reason), reason);
    REQUIRE_MESSAGE(exact_p_key(projective_closure(starred), p, reason), reason);
    CHECK(d.bytes.size() > 6);
    CHECK_FALSE(e.bytes.empty());
    CHECK_FALSE(p.bytes.empty());
}

TEST_CASE("E and P identifiers exist when a star is sent to infinity") {
    const OMatrix starred = matrix_of("0*", 3);
    ProjectiveOMatrix p = projective_closure(starred);

    // Line 1 belongs to the triple point of this fixture.  In that chart its
    // two partners are parallel, yet both exact keys remain defined.
    const RotationResult parallel = chart_of(p, 1);
    REQUIRE_MESSAGE(parallel.ok, parallel.reason);
    const bool has_short_row =
        parallel.o.rows[0].size() < 2 || parallel.o.rows[1].size() < 2 ||
        parallel.o.rows[2].size() < 2;
    CHECK(has_short_row);

    ExactKey e, pk;
    std::string reason;
    REQUIRE_MESSAGE(exact_e_key(parallel.o, e, reason), reason);
    REQUIRE_MESSAGE(exact_p_key(p, pk, reason), reason);
    CHECK_FALSE(e.bytes.empty());
    CHECK_FALSE(pk.bytes.empty());

    const ClassIdentifiers finite_ids = class_identifiers(starred, p);
    const ClassIdentifiers parallel_ids = class_identifiers(parallel.o, p);
    REQUIRE_MESSAGE(finite_ids.ok, finite_ids.reason);
    REQUIRE_MESSAGE(parallel_ids.ok, parallel_ids.reason);
    CHECK(parallel_ids.pid == finite_ids.pid);
    CHECK(parallel_ids.eid.rfind("E3-", 0) == 0);
    CHECK(parallel_ids.did.rfind("D3-", 0) == 0);

    const ClassIdentifiers rebuilt_parallel_ids =
        class_identifiers(parallel.o, projective_closure(parallel.o));
    REQUIRE_MESSAGE(rebuilt_parallel_ids.ok, rebuilt_parallel_ids.reason);
    CHECK(rebuilt_parallel_ids.pid == finite_ids.pid);
}

TEST_CASE("P key is invariant across an E orbit with a finite triple point") {
    const OMatrix first = matrix_of(
        "0 1 0 2 4 3 2 4 7 6 5 4 3 2 1 0 2 4 6* 8 7 6 5 4 3 2 1 0 "
        "2 4 6 5 4 3 2 4 6 7 6 5 4 3 6",
        10);
    const OMatrix second = matrix_of(
        "0 1 3 2 1 3 4 3 6 5 8 6* 5 4 3 2 1 0 1 3 5 4 3 2 1 3 5 8 7 "
        "6 5 4 3 2 1 3 5 7 6 5 4 3 7",
        10);

    ExactKey first_e, second_e, first_p, second_p;
    std::string reason;
    REQUIRE_MESSAGE(exact_e_key(first, first_e, reason), reason);
    REQUIRE_MESSAGE(exact_e_key(second, second_e, reason), reason);
    REQUIRE(first_e.bytes == second_e.bytes);
    REQUIRE_MESSAGE(exact_p_key(projective_closure(first), first_p, reason), reason);
    REQUIRE_MESSAGE(exact_p_key(projective_closure(second), second_p, reason), reason);
    CHECK(first_p.bytes == second_p.bytes);
}

TEST_CASE("traced generalized orbits expose charts and line relabelings") {
    const OMatrix starred = matrix_of("0*", 3);
    const ProjectiveOMatrix form = projective_closure(starred);

    for (int infinity = 0; infinity <= 3; ++infinity) {
        std::vector<ChartVariant> charts;
        std::string reason;
        REQUIRE_MESSAGE(chart_variants(form, infinity, charts, reason), reason);
        REQUIRE_FALSE(charts.empty());
        for (const ChartVariant& chart : charts) {
            CHECK(chart.relabel.size() == 4);
            CHECK(word_from_omatrix(chart.chart).ok);
        }
    }

    std::vector<EuclideanImage> images;
    std::string reason;
    REQUIRE_MESSAGE(euclidean_images(starred, true, images, reason), reason);
    CHECK(images.size() == 12);
    for (const EuclideanImage& image : images) {
        CHECK(image.relabel.size() == 4);
        ExactKey rebuilt;
        REQUIRE_MESSAGE(exact_d_key(image.image, rebuilt, reason), reason);
        CHECK(rebuilt.bytes == image.d_key.bytes);
    }
}

TEST_CASE("the Euclidean orbit of a chart with parallel lines stays replayable") {
    // Line 1 of the triple-point fixture sent to infinity leaves its two
    // partners parallel.  resolve() writes that pair as a crossing just past
    // the right edge, and a rotation either keeps it there or carries it to
    // the left edge, where deleting it has to undo the swap it performed.
    // Rotations that split the pair between the two edges are no chart of
    // this arrangement at all: its two parallel lines run in one direction,
    // not two.
    const OMatrix starred = matrix_of("0*", 3);
    const RotationResult parallel = chart_of(projective_closure(starred), 1);
    REQUIRE_MESSAGE(parallel.ok, parallel.reason);

    std::vector<EuclideanImage> images;
    std::string reason;
    REQUIRE_MESSAGE(euclidean_images(parallel.o, true, images, reason), reason);
    // Two directions instead of three: 4m images, not 4n.
    CHECK(images.size() == 8);
    for (const EuclideanImage& image : images) {
        CAPTURE(image.rotation);
        CAPTURE(image.reflected);
        ExactKey rebuilt;
        // exact_d_key replays the word, so this is the whole claim.
        REQUIRE_MESSAGE(exact_d_key(image.image, rebuilt, reason), reason);
        CHECK(rebuilt.bytes == image.d_key.bytes);
    }

    // A group action: every image has the same orbit.
    std::set<std::vector<std::uint8_t>> keys;
    for (const EuclideanImage& image : images) keys.insert(image.d_key.bytes);
    for (const EuclideanImage& image : images) {
        std::vector<EuclideanImage> again;
        REQUIRE_MESSAGE(euclidean_images(image.image, true, again, reason), reason);
        std::set<std::vector<std::uint8_t>> again_keys;
        for (const EuclideanImage& other : again) again_keys.insert(other.d_key.bytes);
        CHECK(again_keys == keys);
    }
}
