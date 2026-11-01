// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Roman Parpalak and Denis Utkin

#pragma once

#include <array>
#include <string>
#include <vector>

#include "arrangement.hpp"

namespace addline {

// Faces are traced on the orientation double cover and antipodal pairs are
// identified. A line with d events has d edges: d-1 segments and one wrap arc
// through its own point at infinity, which carries the voltage of the cover.
struct Closure {
    struct Face {
        std::vector<int> keys;     // edge ids, in cyclic boundary order
        std::vector<int> corners;  // corners[i] joins keys[i] and keys[i+1]
        int color = 0;

        int size() const { return static_cast<int>(keys.size()); }
    };

    int lines = 0;      // including the line at infinity
    int infinity = 0;   // its label, always lines - 1
    Events events;

    std::vector<int> edge_line;
    std::vector<int> edge_from, edge_to;  // events
    std::vector<char> edge_voltage;       // 1 for the wrap arc

    std::vector<Face> faces;
    std::vector<std::array<int, 2>> edge_faces;   // the two faces of an edge
    std::vector<std::vector<int>> vertex_faces;   // faces incident with a vertex
    // Faces around a vertex in cyclic order: four at a crossing, six at a star.
    std::vector<std::vector<int>> vertex_sectors;
    std::vector<int> face_of_first_dart;          // debug aid, lifted face id

    int edge_count() const { return static_cast<int>(edge_line.size()); }
    int face_count() const { return static_cast<int>(faces.size()); }
};

struct ClosureResult {
    bool ok = false;
    Closure closure;
    std::string reason;
};

// WORD has n affine lines; the closure has n+1.
ClosureResult build_closure(const Word& word, int n);

// EVENTS already include the line at infinity, as after a contraction.
ClosureResult build_closure_from_events(Events events);

}  // namespace addline
