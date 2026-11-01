// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Roman Parpalak and Denis Utkin

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include "line_permutation_group.hpp"
#include "traced_transform.hpp"

#include <vector>

TEST_CASE("Trivial group C1") {
    auto id = LinePermutation::identity(4);
    auto g = LinePermutationGroup::from_elements({id});
    CHECK(g.order() == 1);
    CHECK(g.type() == "C1");
    CHECK(g.is_abelian());
    CHECK(g.generators().empty());
}

TEST_CASE("Cyclic group C3") {
    LinePermutation a({1, 2, 0, 3});         // 3-cycle (0 1 2)
    LinePermutation aa({2, 0, 1, 3});        // a^2
    auto id = LinePermutation::identity(4);
    auto g = LinePermutationGroup::from_elements({id, a, aa});
    CHECK(g.order() == 3);
    CHECK(g.type() == "C3");
    CHECK(g.is_abelian());
    REQUIRE(g.generators().size() == 1);
    CHECK(g.generators()[0].order() == 3);
}

TEST_CASE("Klein four-group D2") {
    auto id = LinePermutation::identity(4);
    LinePermutation a({1, 0, 3, 2});
    LinePermutation b({2, 3, 0, 1});
    LinePermutation c({3, 2, 1, 0});
    auto g = LinePermutationGroup::from_elements({id, a, b, c});
    CHECK(g.order() == 4);
    CHECK(g.type() == "D2");  // Klein four = D_2 in |D_k|=2k convention
    CHECK(g.is_abelian());
    // D2 has no element of order 4, so two generators are needed.
    CHECK(g.generators().size() == 2);
    CHECK(LinePermutationGroup::closure_size(g.generators()) == 4);
}

TEST_CASE("Symmetric group S3 / D3") {
    auto id = LinePermutation::identity(3);
    LinePermutation r1({1, 2, 0});  // 3-cycle (0 1 2)
    LinePermutation r2({2, 0, 1});  // r1^2
    LinePermutation s1({1, 0, 2});  // transposition
    LinePermutation s2({0, 2, 1});
    LinePermutation s3({2, 1, 0});
    auto g = LinePermutationGroup::from_elements({id, r1, r2, s1, s2, s3});
    CHECK(g.order() == 6);
    CHECK(g.type() == "D3/S3");
    CHECK_FALSE(g.is_abelian());
    CHECK(g.generators().size() <= 2);
    CHECK(LinePermutationGroup::closure_size(g.generators()) == 6);
}

TEST_CASE("closure_size on independent set returns full group") {
    LinePermutation a({1, 0, 3, 2});
    LinePermutation b({2, 3, 0, 1});
    CHECK(LinePermutationGroup::closure_size({a, b}) == 4);

    LinePermutation r({1, 2, 0, 3});
    CHECK(LinePermutationGroup::closure_size({r}) == 3);
}

TEST_CASE("closure_size on identity-only generators is 1") {
    CHECK(LinePermutationGroup::closure_size({}) == 1);
}

TEST_CASE("closure returns full element set sorted") {
    LinePermutation r({1, 2, 0, 3});
    auto cl = LinePermutationGroup::closure({r}, 4);
    CHECK(cl.size() == 3);
    // Sorted by std::set order: identity < {1,2,0,3} < {2,0,1,3}
    CHECK(cl[0].as_array() == std::vector<size_t>{0, 1, 2, 3});
    CHECK(cl[1].as_array() == std::vector<size_t>{1, 2, 0, 3});
    CHECK(cl[2].as_array() == std::vector<size_t>{2, 0, 1, 3});
}

TEST_CASE("element order histogram") {
    auto id = LinePermutation::identity(4);
    LinePermutation a({1, 0, 3, 2});  // order 2
    LinePermutation b({2, 3, 0, 1});  // order 2
    LinePermutation c({3, 2, 1, 0});  // order 2
    auto g = LinePermutationGroup::from_elements({id, a, b, c});
    auto hist = g.element_orders();
    CHECK(hist[1] == 1);
    CHECK(hist[2] == 3);
}
