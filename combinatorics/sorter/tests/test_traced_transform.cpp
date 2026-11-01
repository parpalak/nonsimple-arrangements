// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Roman Parpalak and Denis Utkin

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include "omatrix.hpp"
#include "traced_transform.hpp"

#include <vector>

namespace {

std::vector<size_t> as_vec(const LinePermutation& p) { return p.as_array(); }

}  // namespace

TEST_CASE("LinePermutation basic properties") {
    auto id = LinePermutation::identity(6);
    CHECK(id.size() == 6);
    CHECK(id.order() == 1);
    CHECK(id.to_cycles_string() == "()");
    CHECK(id.image(3) == 3);

    LinePermutation p({1, 2, 0, 3});
    CHECK(p.size() == 4);
    CHECK(p.order() == 3);
    CHECK(p.image(0) == 1);
    CHECK(p.image(2) == 0);
    CHECK(p.to_cycles_string() == "(0 1 2)");

    // Inverse: [1,2,0,3] -> [2,0,1,3]
    auto inv = p.inverse();
    CHECK(as_vec(inv) == std::vector<size_t>{2, 0, 1, 3});
    auto round_trip = p.compose(inv);
    CHECK(round_trip == LinePermutation::identity(4));
}

TEST_CASE("LinePermutation compose convention: (g compose h)(x) = h(g(x))") {
    // g = [1, 0, 2] sends 0->1, 1->0, 2->2.
    // h = [0, 2, 1] sends 0->0, 1->2, 2->1.
    // g.compose(h) applies g first, then h:
    //   old 0 -> g(0)=1 -> h(1)=2
    //   old 1 -> g(1)=0 -> h(0)=0
    //   old 2 -> g(2)=2 -> h(2)=1
    LinePermutation g({1, 0, 2});
    LinePermutation h({0, 2, 1});
    auto gh = g.compose(h);
    CHECK(as_vec(gh) == std::vector<size_t>{2, 0, 1});
}

TEST_CASE("traced_identity n=5") {
    std::vector<size_t> gens = {1, 3, 2, 1, 3, 0, 1, 2, 1, 3};
    auto o = get_OMatrix(gens);
    REQUIRE(o.has_value());

    TracedOMatrix t = traced_identity(*o);
    CHECK(t.matrix == *o);
    CHECK(t.old_to_new_line.size() == 6);
    CHECK(as_vec(t.old_to_new_line) == std::vector<size_t>{0, 1, 2, 3, 4, 5});
}

TEST_CASE("traced_projective_rotation induced line permutations on n=5") {
    std::vector<size_t> gens = {1, 3, 2, 1, 3, 0, 1, 2, 1, 3};
    auto o = get_OMatrix(gens);
    REQUIRE(o.has_value());
    auto id = traced_identity(*o);

    // Induced line permutation tau(pi_p; M) on the identity-traced
    // matrix for p = 0..4 (arXiv:2607.29236, (C.8)); cross-verified against
    // an independent reference implementation.
    std::vector<std::vector<size_t>> expected = {
        {5, 3, 1, 2, 0, 4},
        {3, 5, 0, 2, 1, 4},
        {2, 0, 5, 3, 1, 4},
        {2, 1, 3, 5, 0, 4},
        {3, 1, 2, 0, 5, 4},
    };
    for (size_t p = 0; p < 5; ++p) {
        auto t = traced_projective_rotation(id, p);
        CHECK(t.old_to_new_line.size() == 6);
        CHECK(as_vec(t.old_to_new_line) == expected[p]);
    }
}

TEST_CASE("traced_rotate identity-then-rotate on n=5") {
    std::vector<size_t> gens = {1, 3, 2, 1, 3, 0, 1, 2, 1, 3};
    auto o = get_OMatrix(gens);
    REQUIRE(o.has_value());
    auto id = traced_identity(*o);

    // Single Euclidean rotation rho: tau(rho;.) sends i -> (i-1) mod n
    // on finite labels and fixes n (arXiv:2607.29236, (C.7)).
    auto r1 = traced_rotate(id, 1);
    REQUIRE(r1.has_value());
    CHECK(as_vec(r1->old_to_new_line) == std::vector<size_t>{4, 0, 1, 2, 3, 5});

    // rotate(id, 0) is identity.
    auto r0 = traced_rotate(id, 0);
    REQUIRE(r0.has_value());
    CHECK(as_vec(r0->old_to_new_line) == std::vector<size_t>{0, 1, 2, 3, 4, 5});
}

TEST_CASE("traced_rotate composed with traced_projective_rotation on n=5") {
    std::vector<size_t> gens = {1, 3, 2, 1, 3, 0, 1, 2, 1, 3};
    auto o = get_OMatrix(gens);
    REQUIRE(o.has_value());
    auto id = traced_identity(*o);

    // Composition rho^3 . pi_2: project line 2 to infinity, then apply
    // three Euclidean rotations. The induced line permutation follows
    // the groupoid law (arXiv:2607.29236, (5.1)).
    auto proj2 = traced_projective_rotation(id, 2);
    auto then = traced_rotate(proj2, 3);
    REQUIRE(then.has_value());
    CHECK(as_vec(then->old_to_new_line) == std::vector<size_t>{4, 2, 5, 0, 3, 1});
}

TEST_CASE("traced_reflect on n=5 reverses finite labels and fixes infinity") {
    std::vector<size_t> gens = {1, 3, 2, 1, 3, 0, 1, 2, 1, 3};
    auto o = get_OMatrix(gens);
    REQUIRE(o.has_value());
    auto id = traced_identity(*o);

    auto refl = traced_reflect(id);
    // Expected: [4, 3, 2, 1, 0, 5]
    CHECK(as_vec(refl.old_to_new_line) == std::vector<size_t>{4, 3, 2, 1, 0, 5});

    // Applying reflect twice returns to identity.
    auto refl2 = traced_reflect(refl);
    CHECK(as_vec(refl2.old_to_new_line) == std::vector<size_t>{0, 1, 2, 3, 4, 5});
    CHECK(refl2.matrix == *o);
}

TEST_CASE("traced_rotate result is always a valid permutation of size n+1") {
    std::vector<size_t> gens = {1, 3, 2, 1, 3, 0, 1, 2, 1, 3};
    auto o = get_OMatrix(gens);
    REQUIRE(o.has_value());
    auto id = traced_identity(*o);
    const size_t n = o->n();

    for (size_t r = 0; r < 2 * n; ++r) {
        auto t = traced_rotate(id, r);
        if (!t.has_value())
            continue;
        CHECK(t->old_to_new_line.size() == n + 1);
        // Validate (constructor would already throw on bad permutations).
        std::vector<bool> seen(n + 1, false);
        for (size_t v : t->old_to_new_line.as_array()) {
            REQUIRE(v <= n);
            REQUIRE_FALSE(seen[v]);
            seen[v] = true;
        }
    }
}
