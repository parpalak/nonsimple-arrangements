// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Roman Parpalak and Denis Utkin

#include "emit.hpp"

#include <algorithm>
#include <numeric>
#include <vector>

#include "counts.hpp"
#include "identifiers.hpp"
#include "omatrix.hpp"

namespace addline {

namespace {

// Point ordinals of the new crossings, above those of any core row.
constexpr int kFreshPoint = 1 << 20;

struct Extension {
    ProjectiveOMatrix form;
    std::vector<int> label;  // core line -> label in FORM, plus the new line
    int new_label = -1;
};

struct Arrival {
    int after = -1;  // position of the core row the new line follows
    bool join = false;  // it does not follow that crossing, it joins it
};

// The anchor first: the search records it apart from the steps.
std::vector<int> route_elements(const RouteEngine::Witness& witness) {
    std::vector<int> out;
    out.reserve(witness.steps.size() + 1);
    out.push_back(witness.anchor_element);
    for (const RouteEngine::Step& step : witness.steps) out.push_back(step.element);
    return out;
}

// The core event behind every position of the projective rows; a star takes
// several positions naming one event.
bool events_behind_positions(const Closure& core,
                             std::vector<std::vector<int>>& out,
                             std::string& reason) {
    const int lines = core.lines;
    const int n = lines - 1;
    out.assign(static_cast<std::size_t>(lines), {});
    for (int line = 0; line < n; ++line) {
        int at_infinity = -1;
        for (int event : core.events.on[line]) {
            int here = 0;
            for (int other : core.events.at[event]) {
                if (other == line) continue;
                if (other == core.infinity) {
                    at_infinity = event;
                    continue;
                }
                ++here;
            }
            if (here && at_infinity == event) {
                reason = "a star of the core sits on its line at infinity";
                return false;
            }
            for (int i = 0; i < here; ++i) out[line].push_back(event);
        }
        if (at_infinity < 0) {
            reason = "line " + std::to_string(line) + " misses infinity";
            return false;
        }
        out[line].push_back(at_infinity);  // appended by projective_closure
    }

    // The row of the line at infinity is the affine lines in label order.
    std::vector<int> by_line(static_cast<std::size_t>(lines), -1);
    for (int event : core.events.on[core.infinity]) {
        for (int other : core.events.at[event]) {
            if (other != core.infinity) by_line[other] = event;
        }
    }
    for (int line = 0; line < n; ++line) {
        if (by_line[line] < 0) {
            reason = "infinity misses line " + std::to_string(line);
            return false;
        }
        out[core.infinity].push_back(by_line[line]);
    }
    return true;
}

int last_position(const std::vector<int>& events, int event) {
    for (int i = static_cast<int>(events.size()) - 1; i >= 0; --i) {
        if (events[i] == event) return i;
    }
    return -1;
}

int first_position(const std::vector<int>& events, int event) {
    for (std::size_t i = 0; i < events.size(); ++i) {
        if (events[i] == static_cast<int>(event)) return static_cast<int>(i);
    }
    return -1;
}

// No chart yet: a route through the core's own crossing at infinity is no
// special case here.
bool build_extension(const Closure& core, const RouteEngine::Witness& witness,
                     bool reversed, Extension& out, std::string& reason) {
    const int lines = core.lines;
    const int n = lines - 1;
    const int fresh_line = lines;  // the new line, before it is labelled

    const OMatrixResult affine = omatrix_from_closure(core);
    if (!affine.ok) {
        reason = affine.reason;
        return false;
    }
    const ProjectiveOMatrix core_form = projective_closure(affine.o);

    std::vector<std::vector<int>> events;
    if (!events_behind_positions(core, events, reason)) return false;

    std::vector<Arrival> arrival(static_cast<std::size_t>(lines));
    std::vector<int> along_new;      // core lines, in the order the route meets them
    std::vector<int> along_new_point;
    int fresh = kFreshPoint;

    for (int element : route_elements(witness)) {
        if (element % 2 == 1) {  // through a crossing, which becomes a star
            const int event = element / 2;
            if (event < 0 || event >= static_cast<int>(core.events.at.size())) {
                reason = "route passes through an unknown vertex";
                return false;
            }
            const std::vector<int>& wires = core.events.at[event];
            if (wires.size() != 2) {
                reason = "route passes through a crossing that is not simple";
                return false;
            }
            for (int line : wires) {
                if (arrival[line].after >= 0) {
                    reason = "line crossed twice";
                    return false;
                }
                const int at = first_position(events[line], event);
                if (at < 0) {
                    reason = "the crossing is not on the line";
                    return false;
                }
                arrival[line] = Arrival{at, true};
                along_new.push_back(line);
                along_new_point.push_back(fresh);
            }
            ++fresh;
            continue;
        }

        const int key = element / 2;  // through an arc
        const int line = core.edge_line[key];
        if (arrival[line].after >= 0) {
            reason = "line crossed twice";
            return false;
        }
        const int from = last_position(events[line], core.edge_from[key]);
        const int to = first_position(events[line], core.edge_to[key]);
        if (from < 0 || to < 0) {
            reason = "the arc is not on the line";
            return false;
        }
        if ((from + 1) % n == to) {
            arrival[line] = Arrival{from, false};
        } else if ((to + 1) % n == from) {
            arrival[line] = Arrival{to, false};
        } else {
            reason = "the ends of the arc are not consecutive";
            return false;
        }
        along_new.push_back(line);
        along_new_point.push_back(fresh++);
    }

    for (int line = 0; line < lines; ++line) {
        if (arrival[line].after < 0) {
            reason = "line " + std::to_string(line) + " never crossed";
            return false;
        }
    }
    if (static_cast<int>(along_new.size()) != lines) {
        reason = "the route does not meet every line exactly once";
        return false;
    }
    if (reversed) {
        std::reverse(along_new.begin(), along_new.end());
        std::reverse(along_new_point.begin(), along_new_point.end());
    }

    // Every core row with the new line threaded in.
    std::vector<std::vector<int>> row(static_cast<std::size_t>(lines) + 1);
    std::vector<std::vector<int>> point(static_cast<std::size_t>(lines) + 1);
    for (int line = 0; line < lines; ++line) {
        for (int i = 0; i < n; ++i) {
            row[line].push_back(core_form.rows[line][i]);
            point[line].push_back(core_form.point[line][i]);
            if (i == arrival[line].after) {
                row[line].push_back(fresh_line);
                point[line].push_back(arrival[line].join
                                          ? core_form.point[line][i]
                                          : fresh++);
            }
        }
    }
    row[fresh_line] = along_new;
    point[fresh_line] = along_new_point;

    // Labels in the order along the line at infinity, as for the core.
    out.label.assign(static_cast<std::size_t>(lines) + 1, -1);
    for (int i = 0; i <= n; ++i) out.label[row[core.infinity][i]] = i;
    out.label[core.infinity] = n + 1;
    out.new_label = out.label[fresh_line];

    out.form.n = n + 1;
    out.form.rows.assign(static_cast<std::size_t>(n) + 2, {});
    out.form.point.assign(static_cast<std::size_t>(n) + 2, {});
    for (int line = 0; line <= lines; ++line) {
        const int label = out.label[line];
        std::vector<int>& to_row = out.form.rows[label];
        std::vector<int>& to_point = out.form.point[label];
        for (std::size_t i = 0; i < row[line].size(); ++i) {
            to_row.push_back(out.label[row[line][i]]);
            to_point.push_back(point[line][i]);
        }
    }
    return true;
}

// Positions of the multiple points on VAL's row: the parallel classes of the
// chart sending VAL to infinity.
std::vector<std::vector<int>> runs_of(const ProjectiveOMatrix& p, int val) {
    const int n = p.n;
    std::vector<std::vector<int>> runs;
    std::vector<char> done(static_cast<std::size_t>(n), 0);
    for (int i = 0; i < n; ++i) {
        if (done[i]) continue;
        std::vector<int> same;
        for (int j = i; j < n; ++j) {
            if (p.point[val][j] == p.point[val][i]) {
                same.push_back(j);
                done[j] = 1;
            }
        }
        if (same.size() > 1) runs.push_back(std::move(same));
    }
    return runs;
}

// All orders of one run, the one it came in first.
std::vector<std::vector<int>> orders_of(std::size_t size) {
    std::vector<int> perm(size);
    std::iota(perm.begin(), perm.end(), 0);
    std::vector<std::vector<int>> out;
    do {
        out.push_back(perm);
    } while (std::next_permutation(perm.begin(), perm.end()));
    return out;
}

EmitResult emit_in_chart(const Closure& core, int n,
                         const RouteEngine::Witness& witness, int chart_line) {
    EmitResult result;
    const int expected = witness.per_chart.empty()
                             ? -1
                             : witness.per_chart[chart_line];

    for (int reading = 0; reading < 2; ++reading) {
        Extension extension;
        std::string reason;
        if (!build_extension(core, witness, reading == 1, extension, reason)) {
            result.reason = reason;
            return result;  // the route itself is at fault, not the reading
        }
        const int val = extension.label[chart_line];
        const std::vector<std::vector<int>> runs = runs_of(extension.form, val);

        // The projective form does not fix the order inside a parallel class;
        // try all of them, only one replays.
        std::vector<std::vector<std::vector<int>>> orders;
        std::size_t variants = 1;
        for (const std::vector<int>& run : runs) {
            orders.push_back(orders_of(run.size()));
            variants *= orders.back().size();
        }

        const std::vector<int> row_of_val = extension.form.rows[val];
        for (std::size_t variant = 0; variant < variants; ++variant) {
            ProjectiveOMatrix form = extension.form;
            std::size_t rest = variant;
            for (std::size_t r = 0; r < runs.size(); ++r) {
                const std::vector<int>& perm = orders[r][rest % orders[r].size()];
                rest /= orders[r].size();
                for (std::size_t i = 0; i < runs[r].size(); ++i) {
                    form.rows[val][runs[r][i]] = row_of_val[runs[r][perm[i]]];
                }
            }

            const RotationResult chart = chart_of(form, val);
            if (!chart.ok) {
                result.reason = "chart: " + chart.reason;
                continue;
            }
            const WordResult word = word_from_omatrix(chart.o);
            if (!word.ok) {
                result.reason = "word: " + word.reason;
                continue;
            }
            // Rebuilding from the word alone rules out a wrong class order.
            const ClosureResult built = build_closure(word.word, n + 1);
            if (!built.ok) {
                result.reason = "extension closure: " + built.reason;
                continue;
            }
            if (expected >= 0 &&
                count_triangles(built.closure).finite(built.closure.infinity) !=
                    expected) {
                result.reason = "the word does not carry the counted triangles";
                continue;
            }

            // FORM does not orient parallel rows; identifiers need the form
            // rebuilt from the verified chart.
            const ClassIdentifiers ids =
                class_identifiers(chart.o, projective_closure(chart.o));
            if (!ids.ok) {
                result.reason = "identifiers: " + ids.reason;
                continue;
            }

            result.ok = true;
            result.word = word.word;
            result.chart = chart_line;
            result.triangles = expected;
            for (const Step& step : result.word) {
                if (step.k > 2) ++result.stars;
            }
            for (const std::vector<int>& run : runs) {
                const int m = static_cast<int>(run.size());
                result.parallels += m * (m - 1) / 2;
            }
            result.multiple_at_infinity = static_cast<int>(runs.size());
            result.did = ids.did;
            result.eid = ids.eid;
            result.pid = ids.pid;
            result.label_of_line.assign(core.lines, -1);
            for (int line = 0; line < core.lines; ++line) {
                result.label_of_line[line] = chart.relabel[extension.label[line]];
            }
            return result;
        }
    }

    if (result.reason.empty()) result.reason = "the route yields no word";
    return result;
}

}  // namespace

EmitResult word_from_route(const Closure& core, int n,
                           const RouteEngine::Witness& witness) {
    return emit_in_chart(core, n, witness, core.infinity);
}

EmitResult word_in_witness_chart(const Closure& core, int n,
                                 const RouteEngine::Witness& witness,
                                 int min_triangles) {
    std::vector<int> charts;
    for (int l = 0; l < static_cast<int>(witness.per_chart.size()); ++l) {
        if (witness.per_chart[l] >= min_triangles) charts.push_back(l);
    }
    std::stable_sort(charts.begin(), charts.end(), [&](int a, int b) {
        return witness.per_chart[a] > witness.per_chart[b];
    });

    EmitResult result;
    for (int chart : charts) {
        result = emit_in_chart(core, n, witness, chart);
        if (result.ok) return result;
    }
    if (charts.empty()) result.reason = "no chart holds that many triangles";
    return result;
}

}  // namespace addline
