// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Roman Parpalak and Denis Utkin

#include "doctest.h"

#include <algorithm>
#include <fstream>
#include <map>
#include <string>
#include <vector>

#include "closure.hpp"
#include "counts.hpp"
#include "omatrix.hpp"
#include "word.hpp"

using namespace addline;

namespace {

struct Fixture {
    const char* file;
    int n;
    int fixed_charts;  // rotations that reproduce the O-matrix letter for letter
};

// The simple fixtures: an O-matrix has no way to record a multiple point.
// The n=9 core is symmetric enough that two of its charts come back identical;
// the other two configurations have none.
const Fixture kSimple[] = {
    {"core7.txt", 7, 0},
    {"core9.txt", 9, 2},
    {"seed31.simple.txt", 31, 0},
};

Word load(const std::string& name) {
    std::ifstream file(std::string(COMBINATORICS_DATA_DIR) + "/" + name);
    REQUIRE_MESSAGE(file.good(), "missing fixture: " << name);
    std::string line;
    std::getline(file, line);
    ParseResult parsed = parse_word(line);
    REQUIRE_MESSAGE(parsed.ok, parsed.reason);
    return parsed.word;
}

// What a chart-independent look at the projective arrangement can see: the
// faces, the triangles among them, and what every chart is worth.
struct Invariants {
    std::map<int, int> face_sizes;
    int projective_triangles = 0;
    std::vector<int> incident;  // sorted: rotation permutes the charts

    bool operator==(const Invariants& other) const {
        return face_sizes == other.face_sizes &&
               projective_triangles == other.projective_triangles &&
               incident == other.incident;
    }
};

Invariants invariants_of(const Word& word, int n) {
    ClosureResult built = build_closure(word, n);
    REQUIRE_MESSAGE(built.ok, built.reason);
    Invariants out;
    for (const Closure::Face& face : built.closure.faces) ++out.face_sizes[face.size()];
    const TriangleCounts counts = count_triangles(built.closure);
    out.projective_triangles = counts.projective;
    out.incident = counts.incident;
    std::sort(out.incident.begin(), out.incident.end());
    return out;
}

OMatrix omatrix_of(const Word& word, int n) {
    OMatrixResult built = omatrix_from_word(word, n);
    REQUIRE_MESSAGE(built.ok, built.reason);
    return built.o;
}

OMatrix omatrix_of(const std::string& name, int n) {
    return omatrix_of(load(name), n);
}

Word word_of(const OMatrix& o) {
    WordResult built = word_from_omatrix(o);
    REQUIRE_MESSAGE(built.ok, built.reason);
    return built.word;
}

}  // namespace

TEST_CASE("the O-matrix round trips through a word") {
    for (const Fixture& fixture : kSimple) {
        CAPTURE(fixture.file);
        const OMatrix o = omatrix_of(fixture.file, fixture.n);
        CHECK(o.n == fixture.n);
        REQUIRE(static_cast<int>(o.rows.size()) == fixture.n);
        for (const std::vector<int>& row : o.rows) {
            CHECK(static_cast<int>(row.size()) == fixture.n - 1);
        }

        // The word itself need not come back letter for letter: commuting
        // letters are free, so the O-matrix is what has to agree.
        const Word again = word_of(o);
        CHECK(again.size() == simple_length(fixture.n));
        const OMatrix back = omatrix_of(again, fixture.n);
        CHECK(back.rows == o.rows);
    }
}

TEST_CASE("multiple points survive as groups") {
    const char* stars[] = {"core13.txt", "core19.txt", "core31.2stars.txt",
                           "word20.117.txt", "word32.315.txt"};
    const int sizes[] = {13, 19, 31, 20, 32};
    const int points[] = {2, 2, 2, 6, 9};
    for (int i = 0; i < 5; ++i) {
        CAPTURE(stars[i]);
        const Word word = load(stars[i]);
        const OMatrixResult built = omatrix_from_word(word, sizes[i]);
        REQUIRE_MESSAGE(built.ok, built.reason);

        // Every triple point shows up as a run of two on each of its three
        // lines, so the runs of size two count each of them three times.
        int runs = 0;
        for (const std::vector<int>& row : built.o.groups) {
            for (int size : row) {
                CHECK(size <= 2);
                if (size == 2) ++runs;
            }
        }
        CHECK(runs == 3 * points[i]);

        // The word comes back and describes the same arrangement.
        const WordResult again = word_from_omatrix(built.o);
        REQUIRE_MESSAGE(again.ok, again.reason);
        CHECK(again.word.size() == word.size());
        CHECK(invariants_of(again.word, sizes[i]) == invariants_of(word, sizes[i]));
    }
}

TEST_CASE("even n rotates too") {
    // The reference refuses an even number of lines; nothing in the formula
    // needs the parity, and the extension of an odd seed is always even.
    const int sizes[] = {4, 6, 8};
    const Word words[] = {
        {{0, 2}, {1, 2}, {0, 2}, {2, 2}, {1, 2}, {0, 2}},
        {{0, 2}, {1, 2}, {0, 2}, {2, 2}, {1, 2}, {0, 2}, {3, 2}, {2, 2},
         {1, 2}, {0, 2}, {4, 2}, {3, 2}, {2, 2}, {1, 2}, {0, 2}},
        {{0, 2}, {1, 2}, {0, 2}, {2, 2}, {1, 2}, {0, 2}, {3, 2}, {2, 2},
         {1, 2}, {0, 2}, {4, 2}, {3, 2}, {2, 2}, {1, 2}, {0, 2}, {5, 2},
         {4, 2}, {3, 2}, {2, 2}, {1, 2}, {0, 2}, {6, 2}, {5, 2}, {4, 2},
         {3, 2}, {2, 2}, {1, 2}, {0, 2}},
    };
    for (int i = 0; i < 3; ++i) {
        const int n = sizes[i];
        CAPTURE(n);
        const OMatrix o = omatrix_of(words[i], n);
        const Invariants expected = invariants_of(words[i], n);
        for (int val = 0; val < n; ++val) {
            CAPTURE(val);
            const RotationResult r = projective_rotate(o, val);
            REQUIRE_MESSAGE(r.ok, r.reason);
            CHECK(invariants_of(word_of(r.o), n) == expected);
        }
    }
}

TEST_CASE("the rotation keeps the projective arrangement") {
    for (const Fixture& fixture : kSimple) {
        CAPTURE(fixture.file);
        const OMatrix o = omatrix_of(fixture.file, fixture.n);
        const Invariants expected = invariants_of(word_of(o), fixture.n);
        int moved = 0;   // rotations that really did change the chart

        for (int val = 0; val < fixture.n; ++val) {
            CAPTURE(val);
            const RotationResult rotated = projective_rotate(o, val);
            REQUIRE_MESSAGE(rotated.ok, rotated.reason);

            // relabel is a permutation of 0..n sending val to infinity.
            REQUIRE(static_cast<int>(rotated.relabel.size()) == fixture.n + 1);
            CHECK(rotated.relabel[val] == fixture.n);
            std::vector<int> images = rotated.relabel;
            std::sort(images.begin(), images.end());
            std::vector<int> all(fixture.n + 1);
            for (int i = 0; i <= fixture.n; ++i) all[i] = i;
            CHECK(images == all);
            CHECK(rotated.relabel[fixture.n] < fixture.n);

            CHECK(invariants_of(word_of(rotated.o), fixture.n) == expected);
            if (rotated.o.rows != o.rows) ++moved;

            // Sending the old infinity back to infinity undoes the rotation.
            const RotationResult back =
                projective_rotate(rotated.o, rotated.relabel[fixture.n]);
            REQUIRE_MESSAGE(back.ok, back.reason);
            CHECK(invariants_of(word_of(back.o), fixture.n) == expected);

            // The chart comes back, the labels do not: they are read off val
            // again, so the arrangement returns shifted by val+1, with the
            // lines above val swept the other way.
            const int n = fixture.n;
            auto shift = [n, val](int label) { return (label - val - 1 + n) % n; };
            std::vector<std::vector<int>> shifted(n);
            for (int line = 0; line < n; ++line) {
                std::vector<int> row;
                for (int other : o.rows[line]) row.push_back(shift(other));
                if (line > val) std::reverse(row.begin(), row.end());
                shifted[shift(line)] = row;
            }
            CHECK(back.o.rows == shifted);
        }
        CHECK(moved == fixture.n - fixture.fixed_charts);
    }
}

TEST_CASE("the rotation refuses what it cannot handle") {
    const OMatrix o = omatrix_of("core7.txt", 7);
    CHECK_FALSE(projective_rotate(o, -1).ok);
    CHECK_FALSE(projective_rotate(o, 7).ok);

    // A line missing one crossing has to be missed back, or the two rows
    // disagree on whether they meet.
    OMatrix lopsided = o;
    lopsided.rows[0].pop_back();
    const RotationResult refused = projective_rotate(lopsided, 1);
    CHECK_FALSE(refused.ok);
    CHECK(refused.reason.find("disagree") != std::string::npos);
    CHECK_FALSE(word_from_omatrix(lopsided).ok);
}

TEST_CASE("a line through a multiple point goes to infinity as a parallel") {
    // Sending such a line to infinity leaves the others of that point
    // parallel: each drops the rest from its row, and the word is shorter.
    const OMatrix starred = omatrix_of("core19.txt", 19);

    // Which order the parallel lines take is not in the projective form, so
    // one label swap per class may be needed before the chart writes down.
    auto swap_labels = [](OMatrix m, int a, int b) {
        std::swap(m.rows[a], m.rows[b]);
        std::swap(m.groups[a], m.groups[b]);
        for (std::vector<int>& row : m.rows) {
            for (int& entry : row) {
                if (entry == a) entry = b;
                else if (entry == b) entry = a;
            }
        }
        return m;
    };

    int with_parallels = 0;
    for (int val = 0; val < 19; ++val) {
        const RotationResult r = projective_rotate(starred, val);
        REQUIRE(r.ok);

        std::vector<std::pair<int, int>> pairs;
        for (int a = 0; a < r.o.n; ++a) {
            for (int b = a + 1; b < r.o.n; ++b) {
                const std::vector<int>& row = r.o.rows[a];
                if (std::find(row.begin(), row.end(), b) == row.end()) {
                    pairs.push_back({a, b});
                }
            }
        }
        // A parallel pair costs one entry in each of the two rows, and the
        // lines of one point get consecutive labels.
        std::size_t entries = 0;
        for (const std::vector<int>& row : r.o.rows) entries += row.size();
        CHECK(entries == static_cast<std::size_t>(r.o.n) * (r.o.n - 1) -
                             2 * pairs.size());
        for (const std::pair<int, int>& pair : pairs) {
            CHECK(pair.second == pair.first + 1);
        }

        if (pairs.empty()) {
            CHECK(word_from_omatrix(r.o).ok);
            continue;
        }
        ++with_parallels;

        // A line may carry two of those points, and then both classes may
        // need turning over, so every combination is tried.
        bool written = false;
        for (int mask = 0; mask < (1 << pairs.size()) && !written; ++mask) {
            OMatrix candidate = r.o;
            for (std::size_t i = 0; i < pairs.size(); ++i) {
                if (mask & (1 << i)) {
                    candidate = swap_labels(candidate, pairs[i].first,
                                            pairs[i].second);
                }
            }
            written = word_from_omatrix(candidate).ok;
        }
        CHECK(written);
    }
    // The two triple points of an E4 core share a line: five lines, not six.
    CHECK(with_parallels == 5);
}
