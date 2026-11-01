// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Roman Parpalak and Denis Utkin

#include "collapse.hpp"

#include <algorithm>

#include "counts.hpp"
#include "omatrix.hpp"

namespace addline {

const char* verdict_name(Verdict verdict) {
    switch (verdict) {
        case Verdict::Ok: return "ok";
        case Verdict::NoDefects: return "no-defects";
        case Verdict::DefectsApart: return "defects-apart";
        case Verdict::NoPentagon: return "no-pentagon";
        case Verdict::NoQuad: return "no-quad";
        case Verdict::NoRoute: return "no-route";
    }
    return "?";
}

int black_color(const Closure& closure) {
    int per_color[2] = {0, 0};
    for (const Closure::Face& face : closure.faces) {
        if (face.size() == 3 && face.color >= 0) ++per_color[face.color];
    }
    return per_color[1] > per_color[0] ? 1 : 0;
}

std::vector<int> black_defects(const Closure& closure) {
    const int black = black_color(closure);
    std::vector<int> defects;
    for (int face = 0; face < closure.face_count(); ++face) {
        if (closure.faces[face].size() == 4 &&
            closure.faces[face].color == black) {
            defects.push_back(face);
        }
    }
    return defects;
}

bool faces_share_vertex(const Closure& closure, int first, int second) {
    for (int a : closure.faces[first].corners) {
        for (int b : closure.faces[second].corners) {
            if (a == b) return true;
        }
    }
    return false;
}

namespace {

int other_face(const Closure& closure, int key, int face) {
    const std::array<int, 2>& pair = closure.edge_faces[key];
    return pair[0] == face ? pair[1] : pair[0];
}

}  // namespace

std::vector<CollapseCandidate> collapse_candidates(
    const Closure& closure, const std::vector<int>& defects) {
    std::vector<CollapseCandidate> found;
    for (int defect : defects) {
        for (int key : closure.faces[defect].keys) {
            const int pentagon = other_face(closure, key, defect);
            if (closure.faces[pentagon].size() != 5) continue;

            // The pentagon sides next to the shared one.
            const std::vector<int>& keys = closure.faces[pentagon].keys;
            const auto at = std::find(keys.begin(), keys.end(), key);
            if (at == keys.end()) continue;
            const int index = static_cast<int>(at - keys.begin());
            const int m = static_cast<int>(keys.size());
            const int left = keys[(index + m - 1) % m];
            const int right = keys[(index + 1) % m];

            CollapseCandidate candidate;
            candidate.defect = defect;
            candidate.pentagon = pentagon;
            candidate.shared_key = key;
            candidate.triangles = {other_face(closure, left, pentagon),
                                   other_face(closure, right, pentagon)};
            if (candidate.triangles[0] == candidate.triangles[1]) continue;
            if (closure.faces[candidate.triangles[0]].size() != 3) continue;
            if (closure.faces[candidate.triangles[1]].size() != 3) continue;
            found.push_back(candidate);
        }
    }
    return found;
}

namespace {

bool triangle_lines(const Closure& closure, int face, std::array<int, 3>& out,
                    std::string& reason) {
    const Closure::Face& f = closure.faces[face];
    if (f.size() != 3) {
        reason = "not a triangle";
        return false;
    }
    for (int i = 0; i < 3; ++i) out[i] = closure.edge_line[f.keys[i]];
    for (int i = 0; i < 3; ++i) {
        if (out[i] == out[(i + 1) % 3]) {
            reason = "triangle sides repeat a line";
            return false;
        }
    }
    return true;
}

int position_in_row(const std::vector<int>& row, int value) {
    for (std::size_t i = 0; i < row.size(); ++i) {
        if (row[i] == value) return static_cast<int>(i);
    }
    return -1;
}

// Merges, on each line of the triangle, its two crossings with the other two
// lines. They are cyclically consecutive; the side may run through infinity.
bool contract_triangle(ProjectiveOMatrix& p, const std::array<int, 3>& lines,
                       std::vector<std::vector<char>>& merged,
                       std::string& reason) {
    const int n = p.n;
    for (int i = 0; i < 3; ++i) {
        const int line = lines[i];
        const int first = position_in_row(p.rows[line], lines[(i + 1) % 3]);
        const int second = position_in_row(p.rows[line], lines[(i + 2) % 3]);
        if (first < 0 || second < 0) {
            reason = "triangle corner is missing from the crossing order";
            return false;
        }
        if ((first + 1) % n != second && (second + 1) % n != first) {
            reason = "triangle corners are not consecutive on its side";
            return false;
        }
        if (merged[line][first] || merged[line][second]) {
            reason = "the two triangles share a corner";
            return false;
        }
        merged[line][first] = merged[line][second] = 1;
        p.point[line][second] = p.point[line][first];
    }
    return true;
}

// Done on the projective form: a contracted side may cross the line at
// infinity of the seed's chart.
struct Contraction {
    int n = 0;                  // affine lines of any chart of the result
    ProjectiveOMatrix form;
    std::vector<int> charts;    // lines clear of both stars, the seed's first
};

bool contract_form(const Closure& closure, const CollapseCandidate& candidate,
                   Contraction& out, std::string& reason) {
    const int n = closure.lines - 1;

    std::array<std::array<int, 3>, 2> lines;
    for (int i = 0; i < 2; ++i) {
        if (!triangle_lines(closure, candidate.triangles[i], lines[i], reason)) {
            return false;
        }
    }

    const OMatrixResult seed = omatrix_from_closure(closure);
    if (!seed.ok) {
        reason = seed.reason;
        return false;
    }
    ProjectiveOMatrix form = projective_closure(seed.o);

    std::vector<std::vector<char>> merged(static_cast<std::size_t>(n) + 1);
    for (int line = 0; line <= n; ++line) {
        merged[line].assign(static_cast<std::size_t>(n), 0);
    }
    for (int i = 0; i < 2; ++i) {
        if (!contract_triangle(form, lines[i], merged, reason)) return false;
    }

    // The seed's own chart first, so that the common case keeps its labels.
    std::vector<char> on_star(static_cast<std::size_t>(n) + 1, 0);
    for (int i = 0; i < 2; ++i) {
        for (int line : lines[i]) on_star[line] = 1;
    }
    out.n = n;
    out.form = std::move(form);
    out.charts.clear();
    if (!on_star[n]) out.charts.push_back(n);
    for (int line = 0; line < n; ++line) {
        if (!on_star[line]) out.charts.push_back(line);
    }
    return true;
}

// VAL has to come from Contraction::charts: with a line through a star at
// infinity, the other two lines of the star would be parallel.
CoreResult core_in_chart(const Contraction& contraction, int val) {
    CoreResult result;
    const int n = contraction.n;

    const RotationResult chart = chart_of(contraction.form, val);
    if (!chart.ok) {
        result.reason = "chart " + std::to_string(val) + ": " + chart.reason;
        return result;
    }
    const WordResult word = word_from_omatrix(chart.o);
    if (!word.ok) {
        result.reason = "chart " + std::to_string(val) + ": " + word.reason;
        return result;
    }
    ClosureResult built = build_closure(word.word, n);
    if (!built.ok) {
        result.reason = "core closure: " + built.reason;
        return result;
    }

    const std::vector<int> stars = star_events(built.closure, false);
    if (stars.size() != 2) {
        result.reason = "the contraction did not leave two stars";
        return result;
    }

    Core core;
    core.closure = std::move(built.closure);
    core.stars = {stars[0], stars[1]};
    for (int i = 0; i < 2; ++i) {
        core.triangles_at_star[i] = triangles_at(core.closure, core.stars[i]);
    }
    const int high = core.triangles_at_star[0] >= core.triangles_at_star[1] ? 0 : 1;
    const int low = 1 - high;
    core.usable = core.triangles_at_star[high] == 4 &&
                  core.triangles_at_star[low] == 3;
    core.deficient_star = core.stars[low];

    result.ok = true;
    result.core = std::move(core);
    return result;
}

}  // namespace

CoreResult contract(const Closure& closure, const CollapseCandidate& candidate) {
    CoreResult result;
    Contraction contraction;
    if (!contract_form(closure, candidate, contraction, result.reason)) {
        return result;
    }
    for (int val : contraction.charts) {
        result = core_in_chart(contraction, val);
        if (result.ok) return result;
    }
    if (result.reason.empty()) result.reason = "no chart writes the core down";
    return result;
}

}  // namespace addline
