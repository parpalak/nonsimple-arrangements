// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Roman Parpalak and Denis Utkin

#include "omatrix.hpp"

#include <algorithm>
#include <numeric>
#include <utility>

#include "arrangement.hpp"

namespace addline {

namespace {

// Point ordinal of every position of a row; positions of one run share it.
std::vector<int> point_of_position(const OMatrix& o, int line) {
    const std::size_t width = o.rows[line].size();
    std::vector<int> out(width, 0);
    if (o.groups.empty() || o.groups[line].empty()) {
        for (std::size_t i = 0; i < width; ++i) out[i] = static_cast<int>(i);
        return out;
    }
    std::size_t at = 0;
    int point = 0;
    for (int size : o.groups[line]) {
        for (int i = 0; i < size && at < width; ++i) out[at++] = point;
        ++point;
    }
    return out;
}

// A short row means a parallel line, whose row has to miss this one back.
bool check_rows(const OMatrix& o, std::string& reason) {
    if (o.n < 2) {
        reason = "n below 2";
        return false;
    }
    if (static_cast<int>(o.rows.size()) != o.n) {
        reason = "row count does not match n";
        return false;
    }
    std::vector<char> meets(static_cast<std::size_t>(o.n) * o.n, 0);
    for (int line = 0; line < o.n; ++line) {
        const std::vector<int>& row = o.rows[line];
        const std::string at = " on line " + std::to_string(line);
        for (int other : row) {
            const std::size_t key =
                static_cast<std::size_t>(line) * o.n + other;
            if (other < 0 || other >= o.n || other == line || meets[key]) {
                reason = "bad crossing order" + at;
                return false;
            }
            meets[key] = 1;
        }
    }
    for (int a = 0; a < o.n; ++a) {
        for (int b = a + 1; b < o.n; ++b) {
            if (meets[static_cast<std::size_t>(a) * o.n + b] !=
                meets[static_cast<std::size_t>(b) * o.n + a]) {
                reason = "line " + std::to_string(a) + " and line " +
                         std::to_string(b) + " disagree on meeting";
                return false;
            }
        }
    }
    if (!o.groups.empty()) {
        if (static_cast<int>(o.groups.size()) != o.n) {
            reason = "group count does not match n";
            return false;
        }
        for (int line = 0; line < o.n; ++line) {
            int total = 0;
            for (int size : o.groups[line]) {
                if (size < 1) {
                    reason = "empty point on line " + std::to_string(line);
                    return false;
                }
                total += size;
            }
            if (total != static_cast<int>(o.rows[line].size())) {
                reason = "points do not cover line " + std::to_string(line);
                return false;
            }
        }
    }
    return true;
}

}  // namespace

OMatrixResult omatrix_from_word(const Word& word, int n) {
    OMatrixResult result;
    const ReplayResult replayed = replay(word, n);
    if (!replayed.ok) {
        result.reason = replayed.reason;
        return result;
    }
    const Events& events = replayed.events;

    result.o.n = n;
    result.o.rows.assign(static_cast<std::size_t>(n), {});
    result.o.groups.assign(static_cast<std::size_t>(n), {});
    for (int line = 0; line < n; ++line) {
        std::vector<int>& row = result.o.rows[line];
        std::vector<int>& groups = result.o.groups[line];
        row.reserve(static_cast<std::size_t>(n - 1));
        for (int event : events.on[line]) {
            int here = 0;
            for (int other : events.at[event]) {
                if (other == line) continue;
                row.push_back(other);
                ++here;
            }
            groups.push_back(here);  // one point, however many lines meet there
        }
    }
    result.ok = true;
    return result;
}

OMatrixResult omatrix_from_closure(const Closure& closure) {
    OMatrixResult result;
    const int n = closure.lines - 1;
    result.o.n = n;
    result.o.rows.assign(static_cast<std::size_t>(n), {});
    result.o.groups.assign(static_cast<std::size_t>(n), {});
    for (int line = 0; line < n; ++line) {
        for (int event : closure.events.on[line]) {
            int here = 0;
            for (int other : closure.events.at[event]) {
                if (other == line || other == closure.infinity) continue;
                result.o.rows[line].push_back(other);
                ++here;
            }
            if (here) result.o.groups[line].push_back(here);
        }
    }
    if (!check_rows(result.o, result.reason)) return result;
    result.ok = true;
    return result;
}

WordResult word_from_omatrix(const OMatrix& o) {
    WordResult result;
    if (!check_rows(o, result.reason)) return result;

    const int n = o.n;
    std::vector<std::vector<int>> point(static_cast<std::size_t>(n));
    for (int line = 0; line < n; ++line) point[line] = point_of_position(o, line);

    std::vector<int> slots(static_cast<std::size_t>(n));
    std::iota(slots.begin(), slots.end(), 0);
    std::vector<std::size_t> done(static_cast<std::size_t>(n), 0);  // per line

    // The lines of the point a line meets next, itself excluded.
    auto next_point = [&](int line, std::vector<int>& members) {
        members.clear();
        const std::size_t at = done[line];
        if (at >= o.rows[line].size()) return false;
        for (std::size_t j = at; j < o.rows[line].size(); ++j) {
            if (point[line][j] != point[line][at]) break;
            members.push_back(o.rows[line][j]);
        }
        return true;
    };

    std::vector<int> members, others;
    for (;;) {
        bool complete = true;
        for (int line = 0; line < n && complete; ++line) {
            complete = done[line] >= o.rows[line].size();
        }
        if (complete) break;

        bool moved = false;
        for (int pos = 0; pos + 1 < n && !moved; ++pos) {
            const int first = slots[pos];
            if (!next_point(first, members)) continue;
            const int width = static_cast<int>(members.size()) + 1;
            if (pos + width > n) continue;

            // Every line of that point has to be adjacent and waiting for it.
            bool ready = true;
            for (int i = 1; i < width && ready; ++i) {
                const int other = slots[pos + i];
                ready = std::find(members.begin(), members.end(), other) != members.end();
            }
            for (int i = 1; i < width && ready; ++i) {
                const int other = slots[pos + i];
                if (!next_point(other, others)) { ready = false; break; }
                if (static_cast<int>(others.size()) + 1 != width) { ready = false; break; }
                for (int j = 0; j < width && ready; ++j) {
                    if (j == i) continue;
                    ready = std::find(others.begin(), others.end(), slots[pos + j]) != others.end();
                }
            }
            if (!ready) continue;

            for (int i = 0; i < width; ++i) {
                const int line = slots[pos + i];
                next_point(line, others);
                done[line] += others.size();
            }
            std::reverse(slots.begin() + pos, slots.begin() + pos + width);
            result.word.push_back(Step{pos, width});
            moved = true;
        }
        if (!moved) {
            result.reason = "the crossing orders are not realizable: no adjacent "
                            "group may cross next";
            return result;
        }
    }

    // Reversed order, except inside parallel classes: checked pairwise.
    std::vector<int> place(static_cast<std::size_t>(n), 0);
    for (int i = 0; i < n; ++i) place[slots[i]] = i;
    for (int a = 0; a < n; ++a) {
        const std::vector<int>& row = o.rows[a];
        for (int b = a + 1; b < n; ++b) {
            const bool meets = std::find(row.begin(), row.end(), b) != row.end();
            if (meets != (place[a] > place[b])) {
                result.reason = "final order does not match the crossings";
                return result;
            }
        }
    }
    result.ok = true;
    return result;
}

ProjectiveOMatrix projective_closure(const OMatrix& o) {
    // An affine line meets the old infinity after its finite crossings; the
    // old infinity meets the affine lines in label order.
    const int n = o.n;
    const int ext = n;
    std::vector<std::vector<char>> meets(
        static_cast<std::size_t>(n),
        std::vector<char>(static_cast<std::size_t>(n), 0));
    for (int line = 0; line < n; ++line) {
        for (int other : o.rows[line]) meets[line][other] = 1;
    }

    // Components of the missing-crossing relation: the points on the old
    // infinity, a parallel class for each larger than one.
    std::vector<int> infinity_point(static_cast<std::size_t>(n), -1);
    int next_infinity_point = 0;
    for (int line = 0; line < n; ++line) {
        if (infinity_point[line] >= 0) continue;
        infinity_point[line] = next_infinity_point;
        for (int other = line + 1; other < n; ++other) {
            if (!meets[line][other]) infinity_point[other] = next_infinity_point;
        }
        ++next_infinity_point;
    }

    ProjectiveOMatrix p;
    p.n = n;
    p.rows.assign(static_cast<std::size_t>(n) + 1, {});
    p.point.assign(static_cast<std::size_t>(n) + 1, {});
    for (int line = 0; line < n; ++line) {
        p.rows[line] = o.rows[line];
        p.point[line] = point_of_position(o, line);
        int point_at_infinity = 0;
        for (int mark : p.point[line]) {
            point_at_infinity = std::max(point_at_infinity, mark + 1);
        }
        for (int other = 0; other < n; ++other) {
            if (other != line && infinity_point[other] == infinity_point[line]) {
                p.rows[line].push_back(other);
                p.point[line].push_back(point_at_infinity);
            }
        }
        p.rows[line].push_back(ext);
        p.point[line].push_back(point_at_infinity);
    }
    p.rows[ext].resize(static_cast<std::size_t>(n));
    std::iota(p.rows[ext].begin(), p.rows[ext].end(), 0);
    p.point[ext] = std::move(infinity_point);
    return p;
}

RotationResult chart_of(const ProjectiveOMatrix& p, int val) {
    RotationResult result;

    const int n = p.n;
    const int ext = n;  // the line the projective form was built around
    if (n < 2) {
        result.reason = "n below 2";
        return result;
    }
    if (static_cast<int>(p.rows.size()) != n + 1 ||
        static_cast<int>(p.point.size()) != n + 1) {
        result.reason = "row count does not match n";
        return result;
    }
    std::vector<char> seen(static_cast<std::size_t>(n) + 1, 0);
    for (int line = 0; line <= n; ++line) {
        const std::string at = " on line " + std::to_string(line);
        if (static_cast<int>(p.rows[line].size()) != n ||
            p.point[line].size() != p.rows[line].size()) {
            result.reason = "missing crossings" + at;
            return result;
        }
        std::fill(seen.begin(), seen.end(), 0);
        for (int other : p.rows[line]) {
            if (other < 0 || other > n || other == line || seen[other]) {
                result.reason = "bad crossing order" + at;
                return result;
            }
            seen[other] = 1;
        }
    }
    if (val < 0 || val > n) {
        result.reason = "val out of range";
        return result;
    }
    // New labels come from the order along val: once val is the line at
    // infinity, that order is the order of the directions of the other lines,
    // which is what a wiring word numbers its wires by.
    std::vector<int> relabel(static_cast<std::size_t>(n) + 1, -1);
    for (int i = 0; i < n; ++i) relabel[p.rows[val][i]] = i;
    relabel[val] = n;

    const std::vector<std::vector<int>>& full = p.rows;
    const std::vector<std::vector<int>>& spot = p.point;

    result.o.n = n;
    result.o.rows.assign(static_cast<std::size_t>(n), {});
    result.o.groups.assign(static_cast<std::size_t>(n), {});
    for (int i = 0; i < n; ++i) {
        const int line = full[val][i];  // the line that becomes label i
        const std::vector<int>& cycle = full[line];
        const std::vector<int>& marks = spot[line];

        int at = 0;
        while (at < n && cycle[at] != val) ++at;
        if (at == n) {
            result.reason = "line " + std::to_string(line) + " misses val";
            return result;
        }

        // The cycle of LINE is cut at val. Lines that val overtakes in the old
        // sweep run backwards in the new chart; for val == ext nobody does.
        std::vector<int> order;
        order.reserve(static_cast<std::size_t>(n - 1));
        if (val != ext && line < val) {
            for (int j = at; j > 0; --j) order.push_back(j - 1);
            for (int j = n; j > at + 1; --j) order.push_back(j - 1);
        } else {
            for (int j = at + 1; j < n; ++j) order.push_back(j);
            for (int j = 0; j < at; ++j) order.push_back(j);
        }

        std::vector<int>& row = result.o.rows[i];
        std::vector<int>& groups = result.o.groups[i];
        row.reserve(order.size());
        // Lines sharing the point where val cuts this one become parallel to
        // it in this chart, and a row has no place for a line it never meets.
        int prev = -1;
        for (std::size_t j = 0; j < order.size(); ++j) {
            const int spot = order[j];
            if (marks[spot] == marks[at]) continue;
            row.push_back(relabel[cycle[spot]]);
            if (prev >= 0 && marks[spot] == marks[prev]) {
                ++groups.back();
            } else {
                groups.push_back(1);
            }
            prev = spot;
        }
    }

    result.relabel = std::move(relabel);
    result.ok = true;
    return result;
}

RotationResult projective_rotate(const OMatrix& o, int val) {
    RotationResult result;
    if (!check_rows(o, result.reason)) return result;
    if (val < 0 || val >= o.n) {
        result.reason = "val out of range";
        return result;
    }
    return chart_of(projective_closure(o), val);
}

}  // namespace addline
