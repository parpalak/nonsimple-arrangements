// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Roman Parpalak and Denis Utkin

#pragma once

#include <string>

#include "closure.hpp"
#include "route.hpp"
#include "word.hpp"

namespace addline {

// The route is a partial order on the events of the extension; the word is a
// greedy linear extension of it, in the chart the core is given in.
struct EmitResult {
    bool ok = false;
    Word word;          // on n+1 lines, the new one labelled n
    int chart = -1;     // the line the word sends to infinity
    int triangles = 0;  // its finite triangles, in that chart
    int stars = 0;      // multiple points of the word itself
    // A multiple point at infinity makes parallel pairs; the word is one
    // letter short per pair.
    int multiple_at_infinity = 0;
    int parallels = 0;
    // Label of each core line in the word, -1 for the line sent to infinity.
    std::vector<int> label_of_line;
    std::string did;
    std::string eid;
    std::string pid;
    std::string reason;
};

EmitResult word_from_route(const Closure& core, int n,
                           const RouteEngine::Witness& witness);

// The same arrangement in the best chart reaching MIN_TRIANGLES; every chart
// at or above it is tried.
EmitResult word_in_witness_chart(const Closure& core, int n,
                                 const RouteEngine::Witness& witness,
                                 int min_triangles = 0);

}  // namespace addline
