// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Roman Parpalak and Denis Utkin

#include "counts.hpp"

#include <algorithm>

namespace addline {

int TriangleCounts::best_chart() const {
    int best = 0;
    for (std::size_t line = 1; line < incident.size(); ++line) {
        if (incident[line] < incident[best]) best = static_cast<int>(line);
    }
    return best;
}

TriangleCounts count_triangles(const Closure& closure) {
    TriangleCounts counts;
    counts.incident.assign(closure.lines, 0);

    std::vector<char> touched(closure.lines, 0);
    for (const Closure::Face& face : closure.faces) {
        if (face.size() != 3) continue;
        ++counts.projective;
        std::vector<int> lines;
        for (int key : face.keys) lines.push_back(closure.edge_line[key]);
        for (int corner : face.corners) {
            for (int wire : closure.events.at[corner]) lines.push_back(wire);
        }
        for (int line : lines) {
            if (touched[line]) continue;
            touched[line] = 1;
            ++counts.incident[line];
        }
        for (int line : lines) touched[line] = 0;
    }
    return counts;
}

std::vector<int> star_events(const Closure& closure, bool without_infinity) {
    std::vector<int> stars;
    for (std::size_t event = 0; event < closure.events.at.size(); ++event) {
        const std::vector<int>& wires = closure.events.at[event];
        if (wires.size() < 3) continue;
        if (without_infinity &&
            std::find(wires.begin(), wires.end(), closure.infinity) !=
                wires.end()) {
            continue;
        }
        stars.push_back(static_cast<int>(event));
    }
    return stars;
}

int triangles_at(const Closure& closure, int vertex) {
    int total = 0;
    for (int face : closure.vertex_faces[vertex]) {
        if (closure.faces[face].size() == 3) ++total;
    }
    return total;
}

}  // namespace addline
