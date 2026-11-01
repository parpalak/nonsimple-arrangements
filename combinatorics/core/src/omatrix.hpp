// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Roman Parpalak and Denis Utkin

#pragma once

#include <string>
#include <vector>

#include "closure.hpp"
#include "word.hpp"

namespace addline {

// A label missing from a row is a line parallel to it.
struct OMatrix {
    int n = 0;
    std::vector<std::vector<int>> rows;   // rows[l] holds n-1 entries
    // Sizes of the runs of a row that share one point; a triple point is a run
    // of two. Empty means simple. The order inside a run is arbitrary.
    std::vector<std::vector<int>> groups;
};

struct OMatrixResult {
    bool ok = false;
    OMatrix o;
    std::string reason;
};

OMatrixResult omatrix_from_word(const Word& word, int n);

// The n = lines - 1 affine lines, without the line at infinity.
OMatrixResult omatrix_from_closure(const Closure& closure);

struct WordResult {
    bool ok = false;
    Word word;
    std::string reason;
};

// Some word of the commutation class.
WordResult word_from_omatrix(const OMatrix& o);

// RELABEL maps old labels to new over 0..n, label n being the line at
// infinity on either side: relabel[val] == n, and relabel[n] is the old line
// at infinity as an ordinary line of the result.
struct RotationResult {
    bool ok = false;
    OMatrix o;
    std::vector<int> relabel;   // size n+1
    std::string reason;
};

RotationResult projective_rotate(const OMatrix& o, int val);

// n+1 projective lines; line n is the line at infinity of the chart the form
// came from. Rows are cyclic orders.
struct ProjectiveOMatrix {
    int n = 0;                            // affine lines; rows.size() == n + 1
    std::vector<std::vector<int>> rows;   // n entries each, read cyclically
    std::vector<std::vector<int>> point;  // point ordinal of every position
};

ProjectiveOMatrix projective_closure(const OMatrix& o);

// The chart with line VAL at infinity. If VAL runs through a multiple point,
// the other lines of that point come out parallel with consecutive labels in
// an arbitrary order; only one order replays as a word, and finding it is the
// caller's job (see chart_variants()).
RotationResult chart_of(const ProjectiveOMatrix& p, int val);

}  // namespace addline
