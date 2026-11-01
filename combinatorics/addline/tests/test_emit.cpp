// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Roman Parpalak and Denis Utkin

#include "doctest.h"

#include <fstream>
#include <string>

#include "collapse.hpp"
#include "counts.hpp"
#include "emit.hpp"

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

// Rebuilds the arrangement from the emitted word alone and counts again, so
// the ledger of the search is never the source of the reported numbers.
struct Recheck {
    int projective = 0;
    int finite = 0;
    int stars = 0;
    std::size_t letters = 0;
};

Recheck recheck(const Word& word, int n) {
    ClosureResult built = build_closure(word, n);
    REQUIRE_MESSAGE(built.ok, built.reason);
    const TriangleCounts counts = count_triangles(built.closure);
    Recheck out;
    out.projective = counts.projective;
    out.finite = counts.finite(built.closure.infinity);
    out.letters = word.size();
    for (const Step& step : word) {
        if (step.k > 2) ++out.stars;
    }
    return out;
}

}  // namespace

TEST_CASE("a route turns into a word of the extension") {
    SUBCASE("n=19 -> 20") {
        const Closure core = closure_of("core19.txt", 19);
        RouteEngine engine(core);
        const RouteEngine::SearchResult found = engine.search(117, false);
        REQUIRE_FALSE(found.witnesses.empty());

        const EmitResult emitted = word_from_route(core, 19, found.witnesses[0]);
        REQUIRE_MESSAGE(emitted.ok, emitted.reason);
        const Recheck seen = recheck(emitted.word, 20);
        CHECK(seen.finite == 117);
        CHECK(seen.stars == emitted.stars);
        // C(20,2) crossings, each star merging three of them into one letter.
        CHECK(seen.letters == 190 - 2 * seen.stars);
    }

    SUBCASE("n=31 -> 32") {
        const Closure core = closure_of("core31.2stars.txt", 31);
        RouteEngine engine(core);
        const RouteEngine::SearchResult found = engine.search(315, false);
        REQUIRE_FALSE(found.witnesses.empty());

        const EmitResult emitted = word_from_route(core, 31, found.witnesses[0]);
        REQUIRE_MESSAGE(emitted.ok, emitted.reason);
        const Recheck seen = recheck(emitted.word, 32);
        CHECK(seen.finite == 315);
        CHECK(seen.projective == 346);
        CHECK(seen.stars == 9);
        CHECK(seen.letters == 478);
    }
}

TEST_CASE("every witness emits a word, in whichever chart it reaches the target") {
    const Closure core = closure_of("core31.2stars.txt", 31);
    RouteEngine engine(core);
    const RouteEngine::SearchResult found = engine.search(315, true);
    REQUIRE(found.witnesses.size() >= 2);

    int here = 0, elsewhere = 0;
    for (const RouteEngine::Witness& witness : found.witnesses) {
        CAPTURE(witness.chart);
        const EmitResult emitted = word_in_witness_chart(core, 31, witness, 315);
        REQUIRE_MESSAGE(emitted.ok, emitted.reason);
        (emitted.chart == core.infinity ? here : elsewhere)++;

        // Rebuilt from the word alone: the chart it was rotated into is the
        // word's own line at infinity, so the target count must show up there.
        const Recheck seen = recheck(emitted.word, 32);
        CHECK(seen.finite == emitted.triangles);
        CHECK(emitted.triangles >= 315);
        CHECK(seen.projective == 346);
        CHECK(seen.stars == emitted.stars);
        // Routes differ in how many crossings they pass through, so the star
        // count is a property of the route, not of the target.
        CHECK(seen.stars >= 9);
        CHECK(seen.letters == 496 - 2 * seen.stars);
    }
    CHECK(here >= 2);
    CHECK(elsewhere >= 1);
    MESSAGE("witnesses: " << here << " in this chart, " << elsewhere
                          << " rotated into another one");
}

// Regression: emit_in_chart() passed the route builder's intermediate
// projective form to class_identifiers().  At a multiple point on its old
// infinity that form does not yet choose compatible directions for the
// parallel rows, although the emitted affine chart is valid.  Normalizing P
// from the verified chart keeps the route; at n = 19 the old failure cost 22
// of the 443 output classes of the augmentation.
TEST_CASE("P is normalized from the verified chart before emission") {
    const Closure core = closure_of("core13.badinfinity.txt", 13);
    const std::vector<int> defects = black_defects(core);
    REQUIRE(defects.size() == 2);

    RouteEngine engine(core);
    engine.anchor_on(defects[0]);
    const RouteEngine::SearchResult found = engine.search(54, true);
    REQUIRE_FALSE(found.witnesses.empty());

    int lost = 0;
    std::string reason;
    for (const RouteEngine::Witness& witness : found.witnesses) {
        const EmitResult emitted = word_in_witness_chart(core, 13, witness, 54);
        if (!emitted.ok) {
            ++lost;
            reason = emitted.reason;
        }
    }
    CHECK_MESSAGE(lost == 0, "routes without a word: " << lost << ", " << reason);
}
