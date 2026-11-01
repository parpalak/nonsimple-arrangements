// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Roman Parpalak and Denis Utkin

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include <fstream>
#include <map>
#include <sstream>
#include <string>

#include "closure.hpp"
#include "counts.hpp"
#include "omatrix.hpp"
#include "word.hpp"

using namespace addline;

namespace {

Word load(const std::string& name) {
    std::ifstream file(std::string(COMBINATORICS_DATA_DIR) + "/" + name);
    REQUIRE_MESSAGE(file.good(), "missing fixture: " << name);
    std::string line;
    std::getline(file, line);
    ParseResult parsed = parse_word(line);
    REQUIRE_MESSAGE(parsed.ok, parsed.reason);
    return parsed.word;
}

Closure closure_of(const std::string& name, int n) {
    ClosureResult built = build_closure(load(name), n);
    REQUIRE_MESSAGE(built.ok, built.reason);
    return built.closure;
}

std::map<int, int> size_profile(const Closure& c) {
    std::map<int, int> profile;
    for (const Closure::Face& face : c.faces) ++profile[face.size()];
    return profile;
}

// Ascending.
std::vector<int> face_profile_at(const Closure& closure, int vertex) {
    std::vector<int> sizes;
    for (int face : closure.vertex_faces[vertex]) {
        sizes.push_back(closure.faces[face].size());
    }
    std::sort(sizes.begin(), sizes.end());
    return sizes;
}

// Such a line cannot go to infinity: the other two lines of its star would
// become parallel.
std::vector<char> lines_through_stars(const Closure& closure) {
    std::vector<char> flagged(closure.lines, 0);
    for (const std::vector<int>& wires : closure.events.at) {
        if (wires.size() < 3) continue;
        for (int wire : wires) flagged[wire] = 1;
    }
    return flagged;
}

int star_count(const Word& word) {
    int total = 0;
    for (const Step& step : word) total += step.k > 2 ? 1 : 0;
    return total;
}

}  // namespace

TEST_CASE("word parsing reads stars as multiplicities") {
    ParseResult a = parse_word("meta stuff ) 0 1* 2** 3***");
    REQUIRE(a.ok);
    CHECK(a.word.size() == 4);
    CHECK(a.word[1] == Step{1, 3});
    CHECK(a.word[2] == Step{2, 4});
    CHECK(a.word[3] == Step{3, 5});
    CHECK(format_word(a.word) == "0 1* 2** 3***");
    CHECK_FALSE(parse_word("0 2{3}").ok);

    ParseResult b = parse_word("0 117 | 1 3 5");
    REQUIRE(b.ok);
    CHECK(b.word.size() == 3);

    ParseResult c = parse_word("#1 K=15 *int=2 ) 0 1* 2** # note");
    REQUIRE(c.ok);
    CHECK(c.word == Word{{0, 2}, {1, 3}, {2, 4}});

    CHECK_FALSE(parse_word("# comment only").ok);
    CHECK_FALSE(parse_word("1 2 x").ok);
}

TEST_CASE("replay validates a wiring word") {
    const Word core = load("seed31.simple.txt");
    CHECK(core.size() == simple_length(31));
    ReplayResult ok = replay(core, 31);
    REQUIRE_MESSAGE(ok.ok, ok.reason);
    CHECK(ok.events.at.size() == simple_length(31));

    // A letter short is not a broken word: the pair that letter would have
    // crossed reads as parallel.
    Word shorter = core;
    shorter.pop_back();
    const ReplayResult few = replay(shorter, 31);
    REQUIRE_MESSAGE(few.ok, few.reason);
    int mates = 0;
    for (const std::vector<int>& list : few.parallel) mates += list.size();
    CHECK(mates == 2);  // one pair, counted from both ends

    // Crossing twice still is.
    Word broken = core;
    broken.push_back(broken.back());
    CHECK_FALSE(replay(broken, 31).ok);
}

TEST_CASE("closure of the known n=31 core") {
    const Closure c = closure_of("core31.2stars.txt", 31);
    CHECK(c.lines == 32);
    CHECK(c.face_count() == 495);

    const std::map<int, int> profile = size_profile(c);
    CHECK(profile == std::map<int, int>{{3, 328}, {4, 3}, {5, 27},
                                        {6, 118}, {7, 19}});

    const TriangleCounts counts = count_triangles(c);
    CHECK(counts.projective == 328);
    CHECK(counts.finite(c.infinity) == 298);

    const std::vector<int> stars = star_events(c, true);
    REQUIRE(stars.size() == 2);

    // One star already has four triangles among its six sectors, the other
    // has three: that second one is what the new line has to justify.
    std::vector<int> triangles{triangles_at(c, stars[0]), triangles_at(c, stars[1])};
    std::sort(triangles.begin(), triangles.end());
    CHECK(triangles == std::vector<int>{3, 4});

    std::vector<std::vector<int>> profiles{face_profile_at(c, stars[0]),
                                           face_profile_at(c, stars[1])};
    std::sort(profiles.begin(), profiles.end());
    CHECK(profiles == std::vector<std::vector<int>>{{3, 3, 3, 3, 4, 5},
                                                    {3, 3, 3, 4, 5, 7}});
}

TEST_CASE("closure of the n=31 seed the core comes from") {
    const Closure c = closure_of("seed31.simple.txt", 31);
    CHECK(c.face_count() == 497);
    CHECK(size_profile(c) == std::map<int, int>{{3, 328}, {4, 4}, {5, 26},
                                                {6, 120}, {7, 18}, {8, 1}});

    const TriangleCounts counts = count_triangles(c);
    // Contracting the two triangles is exactly triangle-neutral.
    CHECK(counts.projective == 328);
    CHECK(counts.finite(c.infinity) == 298);
    CHECK(star_events(c, true).empty());
}

TEST_CASE("the known 32-315 word and its charts") {
    const Word word = load("word32.315.txt");
    CHECK(star_count(word) == 9);
    const Closure c = closure_of("word32.315.txt", 32);
    CHECK(c.lines == 33);

    const TriangleCounts counts = count_triangles(c);
    CHECK(counts.projective == 346);
    CHECK(counts.finite(c.infinity) == 315);

    // Exactly one line reaches the target, and it is the auxiliary infinity.
    int reaching = 0;
    for (int line = 0; line < c.lines; ++line) {
        if (counts.finite(line) >= 315) ++reaching;
    }
    CHECK(reaching == 1);
    CHECK(counts.best_chart() == c.infinity);

    std::map<int, int> spread;
    for (int line = 0; line < c.lines; ++line) ++spread[counts.incident[line]];
    CHECK(spread == std::map<int, int>{{31, 1}, {32, 15}, {33, 16}, {35, 1}});

    // Every star ends up with four triangles, none of them at infinity.
    const std::vector<int> stars = star_events(c, true);
    REQUIRE(stars.size() == 9);
    for (int star : stars) {
        CHECK(triangles_at(c, star) == 4);
        for (int face : c.vertex_faces[star]) {
            if (c.faces[face].size() != 3) continue;
            for (int key : c.faces[face].keys) {
                CHECK(c.edge_line[key] != c.infinity);
            }
        }
    }

    // The winning chart is star-free, so the word can be written down there.
    CHECK(lines_through_stars(c)[counts.best_chart()] == 0);
}

TEST_CASE("smaller known configurations") {
    const Closure c19 = closure_of("core19.txt", 19);
    CHECK(count_triangles(c19).finite(c19.infinity) == 106);
    CHECK(star_events(c19, true).size() == 2);

    const Closure c20 = closure_of("word20.117.txt", 20);
    CHECK(count_triangles(c20).finite(c20.infinity) == 117);
    CHECK(star_events(c20, true).size() == 6);
}
