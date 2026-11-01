// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Roman Parpalak and Denis Utkin

#include "closure.hpp"

#include <algorithm>
#include <queue>

namespace addline {

namespace {

// A dart is one side of one lifted edge: (rotation slot, sheet).
inline int dart_id(int slot, int sheet) { return 2 * slot + sheet; }

}  // namespace

ClosureResult build_closure(const Word& word, int n) {
    ClosureResult result;

    // Replayed on its own first, to find the parallel classes.
    const ReplayResult affine = replay(word, n);
    if (!affine.ok) {
        result.reason = "affine replay: " + affine.reason;
        return result;
    }
    std::vector<ParallelBlock> blocks;
    if (!parallel_blocks(affine, blocks, result.reason)) return result;

    const Word full = add_line_at_infinity(word, n, blocks);
    ReplayResult replayed = replay(full, n + 1, closure_start(n, blocks));
    if (!replayed.ok) {
        result.reason = "closure replay: " + replayed.reason;
        return result;
    }
    return build_closure_from_events(std::move(replayed.events));
}

ClosureResult build_closure_from_events(Events events) {
    ClosureResult result;

    Closure c;
    c.lines = events.n;
    c.infinity = events.n - 1;
    c.events = std::move(events);

    // Without a word to replay, this is the only validity check.
    {
        const int lines = c.lines;
        std::vector<char> met(static_cast<std::size_t>(lines) * lines, 0);
        for (const std::vector<int>& wires : c.events.at) {
            for (std::size_t i = 0; i < wires.size(); ++i) {
                for (std::size_t j = i + 1; j < wires.size(); ++j) {
                    const std::size_t key =
                        static_cast<std::size_t>(wires[i]) * lines + wires[j];
                    const std::size_t mirror =
                        static_cast<std::size_t>(wires[j]) * lines + wires[i];
                    if (met[key]) {
                        result.reason = "lines meet twice";
                        return result;
                    }
                    met[key] = met[mirror] = 1;
                }
            }
        }
        for (int a = 0; a < lines; ++a) {
            for (int b = a + 1; b < lines; ++b) {
                if (!met[static_cast<std::size_t>(a) * lines + b]) {
                    result.reason = "lines never meet";
                    return result;
                }
            }
        }
    }

    const int vertex_count = static_cast<int>(c.events.at.size());

    // ---------------------------------------------------------------- edges
    // right_edge[line][i] is the arc leaving the i-th event of the line;
    // left_edge is the arc entering it. The last arc wraps around.
    std::vector<std::vector<int>> right_edge(c.lines), left_edge(c.lines);
    for (int line = 0; line < c.lines; ++line) {
        const std::vector<int>& on = c.events.on[line];
        const int deg = static_cast<int>(on.size());
        if (deg < 2) {
            result.reason = "line with fewer than two events";
            return result;
        }
        right_edge[line].assign(deg, -1);
        left_edge[line].assign(deg, -1);
        for (int i = 0; i < deg; ++i) {
            const int id = c.edge_count();
            c.edge_line.push_back(line);
            c.edge_from.push_back(on[i]);
            c.edge_to.push_back(on[(i + 1) % deg]);
            c.edge_voltage.push_back(i + 1 == deg ? 1 : 0);
            right_edge[line][i] = id;
            left_edge[line][(i + 1) % deg] = id;
        }
    }

    // ------------------------------------------------------------ rotations
    // Around an event the boundary arcs appear as: every outgoing arc in wire
    // order, then every incoming arc in the same order.
    std::vector<int> rot_start(vertex_count, 0), rot_len(vertex_count, 0);
    std::vector<int> slot_edge, slot_vertex;
    {
        // index of an event within the event list of a line
        std::vector<std::vector<int>> where(c.lines);
        for (int line = 0; line < c.lines; ++line) {
            where[line].assign(vertex_count, -1);
            for (std::size_t i = 0; i < c.events.on[line].size(); ++i) {
                where[line][c.events.on[line][i]] = static_cast<int>(i);
            }
        }
        for (int v = 0; v < vertex_count; ++v) {
            const std::vector<int>& wires = c.events.at[v];
            rot_start[v] = static_cast<int>(slot_edge.size());
            rot_len[v] = 2 * static_cast<int>(wires.size());
            for (int wire : wires) {
                slot_edge.push_back(right_edge[wire][where[wire][v]]);
                slot_vertex.push_back(v);
            }
            for (int wire : wires) {
                slot_edge.push_back(left_edge[wire][where[wire][v]]);
                slot_vertex.push_back(v);
            }
        }
    }

    // slot of an edge at each of its two endpoints
    const int slot_count = static_cast<int>(slot_edge.size());
    std::vector<int> slot_from(c.edge_count(), -1), slot_to(c.edge_count(), -1);
    for (int slot = 0; slot < slot_count; ++slot) {
        const int e = slot_edge[slot];
        if (slot_vertex[slot] == c.edge_from[e] && slot_from[e] < 0) {
            slot_from[e] = slot;
        } else {
            slot_to[e] = slot;
        }
    }
    for (int e = 0; e < c.edge_count(); ++e) {
        if (slot_from[e] < 0 || slot_to[e] < 0) {
            result.reason = "edge without both endpoints";
            return result;
        }
    }

    // ------------------------------------------------------- face traversal
    auto next_dart = [&](int dart) {
        const int slot = dart / 2, sheet = dart % 2;
        const int e = slot_edge[slot];
        const int v = slot_vertex[slot];
        const bool at_from = (slot == slot_from[e]);
        const int other_slot = at_from ? slot_to[e] : slot_from[e];
        const int other_v = slot_vertex[other_slot];
        (void)v;
        const int other_sheet = sheet ^ c.edge_voltage[e];
        const int base = rot_start[other_v], len = rot_len[other_v];
        const int index = other_slot - base;
        const int step = other_sheet == 0 ? 1 : len - 1;
        const int next_slot = base + (index + step) % len;
        return dart_id(next_slot, other_sheet);
    };

    const int dart_count = 2 * slot_count;
    std::vector<int> face_of(dart_count, -1);
    std::vector<std::vector<int>> lifted_faces;
    for (int start = 0; start < dart_count; ++start) {
        if (face_of[start] >= 0) continue;
        const int face = static_cast<int>(lifted_faces.size());
        std::vector<int> walk;
        int cur = start;
        while (face_of[cur] < 0) {
            face_of[cur] = face;
            walk.push_back(cur);
            cur = next_dart(cur);
        }
        if (cur != start) {
            result.reason = "face traversal did not close";
            return result;
        }
        lifted_faces.push_back(std::move(walk));
    }

    const int lifted_count = static_cast<int>(lifted_faces.size());
    if (2 * vertex_count - 2 * c.edge_count() + lifted_count != 2) {
        result.reason = "double cover is not a sphere";
        return result;
    }

    // ------------------------------------------------------------- antipodes
    std::vector<int> antipode(lifted_count, -1);
    for (int face = 0; face < lifted_count; ++face) {
        const int dart = lifted_faces[face][0];
        const int slot = dart / 2, sheet = dart % 2;
        const int e = slot_edge[slot];
        const int other_slot = (slot == slot_from[e]) ? slot_to[e] : slot_from[e];
        const int other_sheet = sheet ^ c.edge_voltage[e] ^ 1;
        antipode[face] = face_of[dart_id(other_slot, other_sheet)];
    }
    for (int face = 0; face < lifted_count; ++face) {
        if (antipode[antipode[face]] != face) {
            result.reason = "antipodal map is not an involution";
            return result;
        }
    }

    // ---------------------------------------------------------------- colors
    std::vector<int> color(lifted_count, -1);
    for (int start = 0; start < lifted_count; ++start) {
        if (color[start] >= 0) continue;
        color[start] = 0;
        std::queue<int> pending;
        pending.push(start);
        while (!pending.empty()) {
            const int face = pending.front();
            pending.pop();
            for (int dart : lifted_faces[face]) {
                const int slot = dart / 2, sheet = dart % 2;
                const int e = slot_edge[slot];
                const int other_slot =
                    (slot == slot_from[e]) ? slot_to[e] : slot_from[e];
                const int other =
                    face_of[dart_id(other_slot, sheet ^ c.edge_voltage[e])];
                if (other == face) {
                    result.reason = "edge with the same face on both sides";
                    return result;
                }
                if (color[other] < 0) {
                    color[other] = color[face] ^ 1;
                    pending.push(other);
                } else if (color[other] != (color[face] ^ 1)) {
                    result.reason = "face map is not two-colorable";
                    return result;
                }
            }
        }
    }
    // The coloring descends to the projective faces only for an even number
    // of lines; otherwise they stay uncolored.
    const bool colorable = (c.lines % 2 == 0);
    for (int face = 0; face < lifted_count; ++face) {
        if ((color[antipode[face]] == color[face]) != colorable) {
            result.reason = "antipodal colors contradict the line parity";
            return result;
        }
    }

    // ---------------------------------------------------- projective faces
    std::vector<int> projective(lifted_count, -1);
    for (int face = 0; face < lifted_count; ++face) {
        const int partner = antipode[face];
        if (face > partner) continue;
        const int id = c.face_count();
        projective[face] = projective[partner] = id;

        Closure::Face out;
        const std::vector<int>& walk = lifted_faces[face];
        out.keys.reserve(walk.size());
        out.corners.reserve(walk.size());
        for (std::size_t i = 0; i < walk.size(); ++i) {
            out.keys.push_back(slot_edge[walk[i] / 2]);
            // The walk turns at the vertex of the next dart, so that vertex is
            // the corner between this side and the next one.
            out.corners.push_back(slot_vertex[walk[(i + 1) % walk.size()] / 2]);
        }
        out.color = colorable ? color[face] : -1;
        c.faces.push_back(std::move(out));
        c.face_of_first_dart.push_back(face);
    }

    const int expected = 1 + [&] {
        int total = 0;
        for (const std::vector<int>& wires : c.events.at) {
            total += static_cast<int>(wires.size()) - 1;
        }
        return total;
    }();
    if (c.face_count() != expected) {
        result.reason = "unexpected projective face count";
        return result;
    }

    // ------------------------------------------------- incidence structures
    c.edge_faces.assign(c.edge_count(), {-1, -1});
    for (int e = 0; e < c.edge_count(); ++e) {
        const int a = projective[face_of[dart_id(slot_from[e], 0)]];
        const int b = projective[face_of[dart_id(slot_to[e], c.edge_voltage[e])]];
        if (a == b) {
            result.reason = "edge with the same projective face on both sides";
            return result;
        }
        c.edge_faces[e] = {std::min(a, b), std::max(a, b)};
    }

    c.vertex_sectors.assign(vertex_count, {});
    for (int v = 0; v < vertex_count; ++v) {
        c.vertex_sectors[v].reserve(rot_len[v]);
        for (int i = 0; i < rot_len[v]; ++i) {
            c.vertex_sectors[v].push_back(
                projective[face_of[dart_id(rot_start[v] + i, 0)]]);
        }
    }

    c.vertex_faces.assign(vertex_count, {});
    for (int face = 0; face < c.face_count(); ++face) {
        for (int corner : c.faces[face].corners) {
            std::vector<int>& list = c.vertex_faces[corner];
            if (std::find(list.begin(), list.end(), face) == list.end()) {
                list.push_back(face);
            }
        }
    }

    result.ok = true;
    result.closure = std::move(c);
    return result;
}

}  // namespace addline
