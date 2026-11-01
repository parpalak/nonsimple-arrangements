// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Roman Parpalak and Denis Utkin

#include "doctest.h"

#include <algorithm>
#include <fstream>
#include <map>
#include <string>

#include "collapse.hpp"
#include "counts.hpp"
#include "word.hpp"

using namespace addline;

namespace {

Closure closure_of(const std::string& name, int n) {
    std::ifstream file(std::string(COMBINATORICS_DATA_DIR) + "/" + name);
    REQUIRE_MESSAGE(file.good(), "missing fixture: " << name);
    std::string line;
    std::getline(file, line);
    ParseResult parsed = parse_word(line);
    REQUIRE_MESSAGE(parsed.ok, parsed.reason);
    ClosureResult built = build_closure(parsed.word, n);
    REQUIRE_MESSAGE(built.ok, built.reason);
    return built.closure;
}

std::map<int, int> size_profile(const Closure& c) {
    std::map<int, int> profile;
    for (const Closure::Face& face : c.faces) ++profile[face.size()];
    return profile;
}

}  // namespace

TEST_CASE("the n=31 seed has two black defects sharing a vertex") {
    const Closure c = closure_of("seed31.simple.txt", 31);
    const std::vector<int> defects = black_defects(c);
    REQUIRE(defects.size() == 2);
    CHECK(faces_share_vertex(c, defects[0], defects[1]));

    // Both defects are black, and black is the color carrying most triangles.
    const int black = black_color(c);
    for (int defect : defects) CHECK(c.faces[defect].color == black);
}

TEST_CASE("contracting the seed reproduces the known core") {
    const Closure seed = closure_of("seed31.simple.txt", 31);
    const Closure known = closure_of("core31.2stars.txt", 31);
    const std::vector<int> defects = black_defects(seed);
    REQUIRE(defects.size() == 2);

    const std::vector<CollapseCandidate> candidates =
        collapse_candidates(seed, defects);
    REQUIRE_FALSE(candidates.empty());

    int contracted = 0, usable = 0, matching = 0, native = 0;
    for (const CollapseCandidate& candidate : candidates) {
        CoreResult core = contract(seed, candidate);
        if (!core.ok) continue;
        ++contracted;

        // Contraction is exactly triangle neutral, whatever it contracts: two
        // triangles go, the pentagon and a white quadrilateral arrive. That is
        // a projective statement. The finite count is not -- it depends on the
        // chart the core had to be written in, which is not the seed's own
        // whenever a star landed on the seed's line at infinity.
        const TriangleCounts counts = count_triangles(core.core.closure);
        CHECK(counts.projective == 328);
        if (counts.finite(core.core.closure.infinity) == 298) ++native;
        CHECK(star_events(core.core.closure, true).size() == 2);

        if (core.core.usable) ++usable;
        if (size_profile(core.core.closure) == size_profile(known)) {
            ++matching;
            CHECK(core.core.usable);
            std::vector<int> at_star{core.core.triangles_at_star[0],
                                     core.core.triangles_at_star[1]};
            std::sort(at_star.begin(), at_star.end());
            CHECK(at_star == std::vector<int>{3, 4});
            CHECK(triangles_at(core.core.closure, core.core.deficient_star) == 3);
        }
    }
    MESSAGE("candidates " << candidates.size() << ", contracted " << contracted
                          << ", usable " << usable << ", matching known "
                          << matching << ", written in the seed's chart "
                          << native);
    CHECK(contracted > 0);
    CHECK(matching >= 1);
    CHECK(native >= 1);
}

TEST_CASE("verdict names are stable") {
    CHECK(std::string(verdict_name(Verdict::NoDefects)) == "no-defects");
    CHECK(std::string(verdict_name(Verdict::NoQuad)) == "no-quad");
}
