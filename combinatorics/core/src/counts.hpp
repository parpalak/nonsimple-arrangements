// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Roman Parpalak and Denis Utkin

#pragma once

#include <vector>

#include "closure.hpp"

namespace addline {

// A triangle is finite in a chart when the line at infinity misses it, at its
// corners included.
struct TriangleCounts {
    int projective = 0;            // triangular faces of the closure
    std::vector<int> incident;     // per line: triangles meeting it
    int finite(int chart_line) const {
        return projective - incident[chart_line];
    }
    int best_chart() const;
};

TriangleCounts count_triangles(const Closure& closure);

// Events of three or more lines.
std::vector<int> star_events(const Closure& closure, bool without_infinity);

int triangles_at(const Closure& closure, int vertex);

}  // namespace addline
