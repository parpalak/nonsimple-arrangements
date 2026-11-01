// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Roman Parpalak and Denis Utkin

#pragma once

#include <array>
#include <string>
#include <vector>

#include "closure.hpp"
#include "omatrix.hpp"

namespace addline {

// In the order the stages run: the first failing stage is reported.
enum class Verdict {
    Ok,
    NoDefects,      // not exactly two black quadrilaterals
    DefectsApart,   // the two defects share no vertex
    NoPentagon,     // no pentagon across a side of a defect
    NoQuad,         // no contraction leaves a star with four triangles
    NoRoute,        // cores exist, no route reaches the target
};

const char* verdict_name(Verdict verdict);

// The color carrying most triangles.
int black_color(const Closure& closure);

std::vector<int> black_defects(const Closure& closure);

bool faces_share_vertex(const Closure& closure, int first, int second);

// A pentagon across a side of a defect, and the two triangles across the
// pentagon sides adjacent to that side.
struct CollapseCandidate {
    int defect = -1;
    int pentagon = -1;
    int shared_key = -1;              // the side both of them carry
    std::array<int, 2> triangles{-1, -1};
};

std::vector<CollapseCandidate> collapse_candidates(const Closure& closure,
                                                   const std::vector<int>& defects);

struct Core {
    Closure closure;
    std::array<int, 2> stars{-1, -1};
    std::array<int, 2> triangles_at_star{0, 0};
    int deficient_star = -1;   // the star the new line has to justify
    bool usable = false;       // one star has four triangles, the other three
};

struct CoreResult {
    bool ok = false;
    Core core;
    std::string reason;
};

// Contracts the two triangles of CANDIDATE and writes the result down in the
// first admissible chart.
CoreResult contract(const Closure& closure, const CollapseCandidate& candidate);

}  // namespace addline
