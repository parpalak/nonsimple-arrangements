// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Roman Parpalak and Denis Utkin

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include <fstream>
#include <string>

#include "counts.hpp"
#include "route.hpp"
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

}  // namespace

TEST_CASE("DP ceilings of the known cores") {
    struct Gate {
        const char* file;
        int n;
        int core;     // triangles of the core in its own chart
        int target;   // known optimum of the extension
        long nodes;   // positions the DP visits, a regression pin
        long reference;  // what the python engine visits, for the record
    };
    // The port matched the reference node for node until the anchors grew to
    // take in the crossings of the line at infinity, which the reference does
    // not consider at all -- it reaches those routes by rewriting the seed in
    // another chart instead. Both columns are kept: the first as a regression
    // pin, the second to show where the difference comes from.
    const Gate gates[] = {
        {"core7.txt", 7, 11, 15, 833, 464},
        {"core9.txt", 9, 21, 25, 1865, 1013},
        {"core13.txt", 13, 46, 54, 5690, 3000},
        {"core19.txt", 19, 106, 117, 18867, 9784},
        {"core31.2stars.txt", 31, 298, 315, 85467, 43681},
    };

    for (const Gate& gate : gates) {
        CAPTURE(gate.file);
        const Closure c = closure_of(gate.file, gate.n);
        RouteEngine engine(c);
        CHECK(engine.core_per_chart()[c.infinity] == gate.core);

        const RouteEngine::Ceiling ceiling = engine.ceiling();
        REQUIRE(ceiling.any_route);
        // The relaxed bound is exact on every known core, in the chart the
        // core is given in.
        CHECK(ceiling.per_chart[c.infinity] == gate.target);
        // No chart of the same projective configuration does better.
        CHECK(ceiling.per_chart[ceiling.best_chart()] == gate.target);
        CHECK(ceiling.nodes == gate.nodes);
        CHECK(gate.nodes > gate.reference);
    }
}

TEST_CASE("the ceiling is an upper bound, never below the truth") {
    // The 32-315 word is a route through the n=31 core, so the ceiling of
    // that core cannot be below 315; the gate above pins it at exactly 315.
    const Closure core = closure_of("core31.2stars.txt", 31);
    RouteEngine engine(core);
    const RouteEngine::Ceiling ceiling = engine.ceiling();

    const Closure result = closure_of("word32.315.txt", 32);
    const TriangleCounts achieved = count_triangles(result);
    CHECK(ceiling.per_chart[core.infinity] >= achieved.finite(result.infinity));
}

TEST_CASE("exact witness reproduces the known routes") {
    SUBCASE("n=19 -> 20, target 117") {
        const Closure c = closure_of("core19.txt", 19);
        RouteEngine engine(c);
        const RouteEngine::SearchResult found = engine.search(117, false);
        REQUIRE(found.witnesses.size() == 1);
        CHECK(found.witnesses[0].triangles == 117);
        CHECK(found.witnesses[0].per_chart[found.witnesses[0].chart] == 117);
        // The oracle keeps the search microscopic.
        CHECK(found.states < 200);
    }

    SUBCASE("n=31 -> 32, target 315") {
        const Closure c = closure_of("core31.2stars.txt", 31);
        RouteEngine engine(c);
        const RouteEngine::SearchResult found = engine.search(315, false);
        REQUIRE(found.witnesses.size() == 1);
        const RouteEngine::Witness& w = found.witnesses[0];
        CHECK(w.triangles == 315);
        // The known 32-315 words: 25 actions of which seven pass through a
        // crossing, giving seven new stars on top of the two in the core.
        CHECK(w.steps.size() + 1 == 25);
        int stars = 0;
        for (const RouteEngine::Step& step : w.steps) {
            if (step.element % 2 == 1) ++stars;
        }
        CHECK(stars == 7);
        CHECK(w.chart == c.infinity);
        CHECK(found.states < 200);
    }

    SUBCASE("nothing above the ceiling") {
        const Closure c = closure_of("core31.2stars.txt", 31);
        RouteEngine engine(c);
        CHECK(engine.search(316, false).witnesses.empty());
    }
}
