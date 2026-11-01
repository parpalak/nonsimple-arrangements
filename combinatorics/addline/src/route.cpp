// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Roman Parpalak and Denis Utkin

#include "route.hpp"

#include <algorithm>
#include <array>
#if defined(__AVX512BW__) && defined(__AVX512F__)
#include <immintrin.h>
#endif

#include "counts.hpp"

namespace addline {

namespace {

constexpr int kNone = -1000000;
// Below any reachable per-chart value, so the DP maximum needs no live flag.
constexpr std::int16_t kUnset = -30000;

std::uint64_t lines_at(const Closure& c, int vertex) {
    std::uint64_t mask = 0;
    for (int wire : c.events.at[vertex]) mask |= 1ull << wire;
    return mask;
}

// A cut piece or a lost triangle counts on every chart except the lines it
// meets: the constant goes into the bulk term, only the set bits are touched.
template <class T>
inline void correct(std::uint64_t mask, int delta, T* values) {
    for (std::uint64_t m = mask; m != 0; m &= m - 1) {
        values[__builtin_ctzll(m)] += delta;
    }
}

#if defined(__AVX512BW__) && defined(__AVX512F__)
#define ADDLINE_WIDE 1

// Up to 32 charts of int16 fit one 512-bit register, and the scattered
// correction becomes one masked add instead of load-modify-stores in memory.
inline __m512i splat_lanes(int value) {
    return _mm512_set1_epi16(static_cast<short>(value));
}

inline __m512i correct_lanes(__m512i lanes, std::uint64_t mask, int delta) {
    return _mm512_mask_add_epi16(lanes, static_cast<__mmask32>(mask), lanes,
                                 splat_lanes(delta));
}
#endif

}  // namespace

int RouteEngine::Ceiling::best_chart() const {
    int best = 0;
    for (std::size_t l = 1; l < per_chart.size(); ++l) {
        if (per_chart[l] > per_chart[best]) best = static_cast<int>(l);
    }
    return best;
}

RouteEngine::RouteEngine(const Closure& closure) : c_(closure) {
    memo_.init(1u << 13);  // a single anchor memoizes a few thousand states
    lines_ = c_.lines;
    full_ = lines_ >= 64 ? ~0ull : ((1ull << lines_) - 1);

    const TriangleCounts counts = count_triangles(c_);
    base_.assign(lines_, 0);
    for (int l = 0; l < lines_; ++l) base_[l] = counts.finite(l);

    geometry_.resize(c_.face_count());
    for (int f = 0; f < c_.face_count(); ++f) {
        const Closure::Face& face = c_.faces[f];
        FaceGeometry& g = geometry_[f];
        g.m = face.size();
        for (std::size_t i = 0; i < face.corners.size(); ++i) {
            for (std::size_t j = i + 1; j < face.corners.size(); ++j) {
                if (face.corners[i] == face.corners[j]) g.dups.push_back(face.corners[i]);
            }
        }
        g.is_triangle = g.m == 3;
        if (g.is_triangle) {
            for (int key : face.keys) g.triangle_lines |= 1ull << c_.edge_line[key];
            for (int corner : face.corners) g.triangle_lines |= lines_at(c_, corner);
        }
    }

    // Moves out of every face: through a side, or diagonally through a simple
    // vertex into the opposite sector.
    moves_.assign(c_.face_count(), {});
    for (int key = 0; key < c_.edge_count(); ++key) {
        const std::array<int, 2>& pair = c_.edge_faces[key];
        const std::uint64_t bit = 1ull << c_.edge_line[key];
        moves_[pair[0]].push_back({pair[1], side_element(key), -1, -1, bit});
        moves_[pair[1]].push_back({pair[0], side_element(key), -1, -1, bit});
    }
    for (std::size_t v = 0; v < c_.events.at.size(); ++v) {
        if (c_.events.at[v].size() != 2) continue;  // a star is impassable
        const std::vector<int>& sectors = c_.vertex_sectors[v];
        if (sectors.size() != 4) continue;
        const std::uint64_t bits = lines_at(c_, static_cast<int>(v));
        for (int i = 0; i < 4; ++i) {
            const int from = sectors[i], to = sectors[(i + 2) % 4];
            if (from == to) continue;
            const Move move{to, vertex_element(static_cast<int>(v)), -1, -1, bits};
            bool known = false;
            for (const Move& other : moves_[from]) {
                if (other.to_face == move.to_face && other.element == move.element) {
                    known = true;
                    break;
                }
            }
            if (!known) moves_[from].push_back(move);
        }
    }

    // Filtered once here rather than in every DP node; the order is kept, so
    // the first witness does not change.
    for (int f = 0; f < c_.face_count(); ++f) {
        std::vector<Move>& list = moves_[f];
        std::size_t keep = 0;
        for (const Move& move : list) {
            if (move.to_face == f) continue;
            if (is_vertex(move.element)) {
                const int vertex = element_value(move.element);
                const std::vector<int>& here = geometry_[f].dups;
                const std::vector<int>& there = geometry_[move.to_face].dups;
                if (std::find(here.begin(), here.end(), vertex) != here.end()) continue;
                if (std::find(there.begin(), there.end(), vertex) != there.end()) continue;
            }
            list[keep++] = move;
        }
        list.resize(keep);
    }

    // Local numbering of the elements a face can be left or entered by, and
    // the cut table laid out on it.
    face_elems_.assign(c_.face_count(), {});
    cut_rows_.assign(c_.face_count(), {});
    for (int f = 0; f < c_.face_count(); ++f) {
        for (Move& move : moves_[f]) {
            std::vector<int>& elems = face_elems_[f];
            auto at = std::find(elems.begin(), elems.end(), move.element);
            if (at == elems.end()) {
                move.col = static_cast<int>(elems.size());
                elems.push_back(move.element);
            } else {
                move.col = static_cast<int>(at - elems.begin());
            }
        }
    }
    std::size_t total = 0;
    for (int f = 0; f < c_.face_count(); ++f) {
        const std::size_t k = face_elems_[f].size();
        cut_rows_[f] = {static_cast<int>(total), static_cast<int>(k)};
        total += k * k;
    }
    Cut blank;
    blank.pieces = -1;  // not computed yet
    cut_table_.assign(total, blank);
    for (int f = 0; f < c_.face_count(); ++f) {
        for (Move& move : moves_[f]) {
            move.to_index = element_index(move.to_face, move.element);
        }
    }
}

int RouteEngine::element_index(int face, int element) const {
    const std::vector<int>& elems = face_elems_[face];
    for (std::size_t i = 0; i < elems.size(); ++i) {
        if (elems[i] == element) return static_cast<int>(i);
    }
    return -1;
}

const RouteEngine::Cut& RouteEngine::cut(int face, int entry, int entry_index,
                                         int exit, int col, Cut& scratch) {
    if (entry_index < 0 || col < 0) {
        scratch = compute_cut(face, entry, exit);
        return scratch;
    }
    const CutRows& rows = cut_rows_[face];
    Cut& slot = cut_table_[rows.base + entry_index * rows.k + col];
    if (slot.pieces < 0) slot = compute_cut(face, entry, exit);
    return slot;
}

int RouteEngine::position_of(int face, int element) const {
    const Closure::Face& f = c_.faces[face];
    if (is_vertex(element)) {
        const int vertex = element_value(element);
        for (std::size_t i = 0; i < f.corners.size(); ++i) {
            if (f.corners[i] == vertex) return static_cast<int>(2 * i + 2) % (2 * f.size());
        }
        return -1;
    }
    const int key = element_value(element);
    for (std::size_t i = 0; i < f.keys.size(); ++i) {
        if (f.keys[i] == key) return static_cast<int>(2 * i + 1);
    }
    return -1;
}

RouteEngine::Cut RouteEngine::compute_cut(int face, int entry, int exit) const {
    const Closure::Face& f = c_.faces[face];
    const int cycle = 2 * f.size();
    const int pos_a = position_of(face, entry), pos_b = position_of(face, exit);

    Cut result;
    // Lines the chord itself already meets: the sides or vertices it starts
    // and ends at.
    std::uint64_t ends = 0;
    int partials = 0;
    for (int element : {entry, exit}) {
        if (is_vertex(element)) {
            ends |= lines_at(c_, element_value(element));
        } else {
            ends |= 1ull << c_.edge_line[element_value(element)];
            ++partials;
        }
    }

    if (pos_a >= 0 && pos_b >= 0 && pos_a != pos_b) {
        for (int pass = 0; pass < 2; ++pass) {
            const int start = pass == 0 ? pos_a : pos_b;
            const int stop = pass == 0 ? pos_b : pos_a;
            int fulls = 0;
            std::uint64_t met = ends;
            for (int at = (start + 1) % cycle; at != stop; at = (at + 1) % cycle) {
                if (at % 2 == 1) {
                    ++fulls;
                    met |= 1ull << c_.edge_line[f.keys[(at - 1) / 2]];
                } else {
                    const int corner = at == 0 ? f.size() - 1 : (at - 2) / 2;
                    met |= lines_at(c_, f.corners[corner]);
                }
            }
            if (fulls + partials != 2) continue;
            result.lines[result.pieces++] = met;
        }
    }

    return result;
}

void RouteEngine::apply_loss(int face, Val* values) const {
    const FaceGeometry& g = geometry_[face];
    if (!g.is_triangle) return;
    for (int l = 0; l < lines_; ++l) --values[l];
    correct(g.triangle_lines, 1, values);
}

int RouteEngine::solve(int face, int element, int entry_index,
                       std::uint64_t crossed) {
    const MemoKey key{(static_cast<std::uint64_t>(face) << 32) |
                          static_cast<std::uint32_t>(element),
                      crossed};
    if (const int* known = memo_.find(key)) return *known;

    ++nodes_;
    std::array<Val, 64> acc;
    std::array<Val, 64> step;
    bool any = false;
    Cut scratch_a, scratch_b;
    for (int l = 0; l < lines_; ++l) acc[l] = kUnset;
#ifdef ADDLINE_WIDE
    // Masked: a plain load would run past the end of the last row of VALUES_.
    const bool wide = lines_ <= 32;
    const __mmask32 live = lines_ >= 32
                               ? static_cast<__mmask32>(~0u)
                               : static_cast<__mmask32>((1u << lines_) - 1);
    __m512i vacc = splat_lanes(kUnset);
#endif

    for (const Move& move : moves_[face]) {
        if (crossed & move.bits) continue;
        if (move.to_face == anchor_after_) continue;

        if (move.to_face == anchor_before_) {
            if (anchor_close_ >= 0 && move.element != anchor_close_) continue;
            if ((crossed | move.bits) != full_) continue;
            const Cut& a =
                cut(face, element, entry_index, move.element, move.col, scratch_a);
            const Cut& b = cut(anchor_before_, move.element, move.to_index,
                               anchor_element_, anchor_exit_col_, scratch_b);
            const int bulk = a.pieces + b.pieces;
#ifdef ADDLINE_WIDE
            if (wide) {
                __m512i vstep = splat_lanes(bulk);
                for (int p = 0; p < a.pieces; ++p) {
                    vstep = correct_lanes(vstep, a.lines[p], -1);
                }
                for (int p = 0; p < b.pieces; ++p) {
                    vstep = correct_lanes(vstep, b.lines[p], -1);
                }
                vacc = _mm512_max_epi16(vacc, vstep);
                any = true;
                continue;
            }
#endif
            for (int l = 0; l < lines_; ++l) step[l] = static_cast<Val>(bulk);
            for (int p = 0; p < a.pieces; ++p) correct(a.lines[p], -1, step.data());
            for (int p = 0; p < b.pieces; ++p) correct(b.lines[p], -1, step.data());
            for (int l = 0; l < lines_; ++l) {
                if (step[l] > acc[l]) acc[l] = step[l];
            }
            any = true;
            continue;
        }

        // The oracle is memoized, so asking it first costs one probe and
        // saves building the cut on every branch that is already dead.
        const int tail = solve(move.to_face, move.element, move.to_index,
                               crossed | move.bits);
        if (tail < 0) continue;
        const Cut& a =
            cut(face, element, entry_index, move.element, move.col, scratch_a);
        const FaceGeometry& g = geometry_[move.to_face];
        const int bulk = a.pieces - (g.is_triangle ? 1 : 0);
        const Val* rest = &values_[static_cast<std::size_t>(tail) * lines_];
#ifdef ADDLINE_WIDE
        if (wide) {
            __m512i vstep = _mm512_add_epi16(
                _mm512_maskz_loadu_epi16(live, rest), splat_lanes(bulk));
            for (int p = 0; p < a.pieces; ++p) {
                vstep = correct_lanes(vstep, a.lines[p], -1);
            }
            if (g.is_triangle) vstep = correct_lanes(vstep, g.triangle_lines, 1);
            vacc = _mm512_max_epi16(vacc, vstep);
            any = true;
            continue;
        }
#endif
        for (int l = 0; l < lines_; ++l) step[l] = static_cast<Val>(bulk + rest[l]);
        for (int p = 0; p < a.pieces; ++p) correct(a.lines[p], -1, step.data());
        if (g.is_triangle) correct(g.triangle_lines, 1, step.data());
        for (int l = 0; l < lines_; ++l) {
            if (step[l] > acc[l]) acc[l] = step[l];
        }
        any = true;
    }

    if (!any) {
        memo_.insert(key, -1);  // a dead end is cached, not left absent
        return -1;
    }
#ifdef ADDLINE_WIDE
    if (wide) _mm512_mask_storeu_epi16(acc.data(), live, vacc);
#endif
    const int slot = static_cast<int>(values_.size() / lines_);
    values_.insert(values_.end(), acc.begin(), acc.begin() + lines_);
    memo_.insert(key, slot);
    return slot;
}

void RouteEngine::walk(int face, int element, int entry_index,
                       std::uint64_t crossed, const Val* acc, SearchResult& out) {
    ++out.states;
    std::array<Val, 64> step;
    std::array<Val, 64> next;
    Cut scratch_a, scratch_b;

    for (const Move& move : moves_[face]) {
        if (crossed & move.bits) continue;
        if (move.to_face == anchor_after_) continue;

        if (move.to_face == anchor_before_) {
            if (anchor_close_ >= 0 && move.element != anchor_close_) continue;
            if ((crossed | move.bits) != full_) continue;
            const Cut& a =
                cut(face, element, entry_index, move.element, move.col, scratch_a);
            const Cut& b = cut(anchor_before_, move.element, move.to_index,
                               anchor_element_, anchor_exit_col_, scratch_b);
            const int bulk = a.pieces + b.pieces;
            for (int l = 0; l < lines_; ++l) step[l] = static_cast<Val>(bulk);
            for (int p = 0; p < a.pieces; ++p) correct(a.lines[p], -1, step.data());
            for (int p = 0; p < b.pieces; ++p) correct(b.lines[p], -1, step.data());
            int chart = -1, best = 0;
            for (int l = 0; l < lines_; ++l) {
                const int total = base_[l] + acc[l] + step[l];
                if (total >= target_ && (chart < 0 || total > best)) {
                    chart = l;
                    best = total;
                }
            }
            if (chart < 0) continue;
            Witness found;
            found.anchor_element = anchor_element_;
            found.steps = path_;
            found.steps.push_back({face, move.element});
            found.per_chart.assign(lines_, 0);
            for (int l = 0; l < lines_; ++l) {
                found.per_chart[l] = base_[l] + acc[l] + step[l];
            }
            found.chart = chart;
            found.triangles = best;
            out.witnesses.push_back(std::move(found));
            if (!all_witnesses_) return;
            continue;
        }

        if (visited_[move.to_face]) continue;

        const int slot = solve(move.to_face, move.element, move.to_index,
                               crossed | move.bits);
        if (slot < 0) continue;
        const Cut& a =
            cut(face, element, entry_index, move.element, move.col, scratch_a);
        const FaceGeometry& g = geometry_[move.to_face];
        const int bulk = a.pieces - (g.is_triangle ? 1 : 0);
        for (int l = 0; l < lines_; ++l) next[l] = static_cast<Val>(acc[l] + bulk);
        for (int p = 0; p < a.pieces; ++p) correct(a.lines[p], -1, next.data());
        if (g.is_triangle) correct(g.triangle_lines, 1, next.data());

        const Val* rest = &values_[static_cast<std::size_t>(slot) * lines_];
        bool alive = false;
        for (int l = 0; l < lines_; ++l) {
            if (base_[l] + next[l] + rest[l] >= target_) {
                alive = true;
                break;
            }
        }
        if (!alive) continue;

        visited_[move.to_face] = 1;
        path_.push_back({face, move.element});
        walk(move.to_face, move.element, move.to_index, crossed | move.bits,
             next.data(), out);
        path_.pop_back();
        visited_[move.to_face] = 0;
        if (!all_witnesses_ && !out.witnesses.empty()) return;
    }
}

RouteEngine::SearchResult RouteEngine::search(int target, bool all_witnesses) {
    SearchResult out;
    target_ = target;
    all_witnesses_ = all_witnesses;
    visited_.assign(c_.face_count(), 0);

    std::array<Val, 64> acc;
    for (const Anchor& anchor : anchors()) {
        anchor_element_ = anchor.element;
        anchor_before_ = anchor.before;
        anchor_after_ = anchor.after;
        anchor_close_ = anchor.close;
        anchor_exit_col_ = element_index(anchor_before_, anchor_element_);
        if (anchor_exit_col_ < 0) continue;
        memo_.clear();
        values_.clear();

        // The oracle for this anchor, and a cheap feasibility test with it.
        const int anchor_entry = element_index(anchor_after_, anchor_element_);
        if (anchor_entry < 0) continue;
        const int slot = solve(anchor_after_, anchor_element_, anchor_entry,
                               anchor.bits);
        if (slot < 0) continue;
        for (int l = 0; l < lines_; ++l) acc[l] = 0;
        apply_loss(anchor_after_, acc.data());
        apply_loss(anchor_before_, acc.data());
        const Val* head = &values_[static_cast<std::size_t>(slot) * lines_];
        bool alive = false;
        for (int l = 0; l < lines_; ++l) {
            if (base_[l] + acc[l] + head[l] >= target) {
                alive = true;
                break;
            }
        }
        if (!alive) continue;

        std::fill(visited_.begin(), visited_.end(), 0);
        visited_[anchor_after_] = 1;
        path_.clear();
        walk(anchor_after_, anchor_element_, anchor_entry, anchor.bits,
             acc.data(), out);
        if (!all_witnesses && !out.witnesses.empty()) break;
    }
    out.dp_nodes = nodes_;
    return out;
}

// The route meets the line at infinity once, through a side or a crossing.
// Crossing anchors have no word in this chart but are kept: without them the
// search would depend on the chart the core is given in.
void RouteEngine::anchor_on_star_cuts(int star) {
    chords_.clear();
    anchor_face_ = -1;
    if (star < 0) return;
    for (int face : c_.vertex_sectors[star]) {
        const Closure::Face& f = c_.faces[face];
        if (f.size() == 3) continue;  // already a triangle, nothing to cut
        for (std::size_t i = 0; i < f.corners.size(); ++i) {
            if (f.corners[i] != star) continue;
            chords_.push_back(Chord{face, f.keys[i],
                                    f.keys[(i + 1) % f.keys.size()]});
        }
    }
}

std::vector<RouteEngine::Anchor> RouteEngine::anchors() const {
    std::vector<Anchor> out;
    if (!chords_.empty()) {
        // Anchor on one side, close through the other; both orientations.
        for (const Chord& chord : chords_) {
            for (int flip = 0; flip < 2; ++flip) {
                const int in = flip ? chord.side_b : chord.side_a;
                const int out_key = flip ? chord.side_a : chord.side_b;
                Anchor anchor;
                anchor.element = side_element(in);
                anchor.before = chord.face;
                anchor.after = c_.edge_faces[in][0] == chord.face
                                   ? c_.edge_faces[in][1]
                                   : c_.edge_faces[in][0];
                anchor.bits = 1ull << c_.edge_line[in];
                anchor.close = side_element(out_key);
                if (anchor.after != anchor.before) out.push_back(anchor);
            }
        }
        return out;
    }
    if (anchor_face_ >= 0) {
        // Every element of the face, taken as the one the route enters by.
        const Closure::Face& face = c_.faces[anchor_face_];
        for (int key : face.keys) {
            Anchor anchor;
            anchor.element = side_element(key);
            anchor.before = c_.edge_faces[key][0] == anchor_face_
                                ? c_.edge_faces[key][1]
                                : c_.edge_faces[key][0];
            anchor.after = anchor_face_;
            anchor.bits = 1ull << c_.edge_line[key];
            out.push_back(anchor);
        }
        for (int corner : face.corners) {
            if (anchor_sides_only_) break;
            if (c_.events.at[corner].size() != 2) continue;
            const std::vector<int>& sectors = c_.vertex_sectors[corner];
            if (sectors.size() != 4) continue;
            std::uint64_t bits = 0;
            for (int wire : c_.events.at[corner]) bits |= 1ull << wire;
            for (int i = 0; i < 4; ++i) {
                if (sectors[i] != anchor_face_) continue;
                Anchor anchor;
                anchor.element = vertex_element(corner);
                anchor.before = sectors[(i + 2) % 4];
                anchor.after = anchor_face_;
                anchor.bits = bits;
                if (anchor.before != anchor.after) out.push_back(anchor);
            }
        }
        return out;
    }
    for (int key = 0; key < c_.edge_count(); ++key) {
        if (c_.edge_line[key] != c_.infinity) continue;
        Anchor anchor;
        anchor.element = side_element(key);
        anchor.before = c_.edge_faces[key][0];
        anchor.after = c_.edge_faces[key][1];
        anchor.bits = 1ull << c_.infinity;
        out.push_back(anchor);
    }
    for (int event : c_.events.on[c_.infinity]) {
        const std::vector<int>& wires = c_.events.at[event];
        if (wires.size() != 2) continue;  // a star is impassable
        const std::vector<int>& sectors = c_.vertex_sectors[event];
        if (sectors.size() != 4) continue;
        std::uint64_t bits = 0;
        for (int line : wires) bits |= 1ull << line;
        // The new line leaves through the sector opposite the one it entered;
        // the two diagonals are the two ways through the crossing.
        for (int i = 0; i < 2; ++i) {
            Anchor anchor;
            anchor.element = vertex_element(event);
            anchor.before = sectors[i];
            anchor.after = sectors[i + 2];
            anchor.bits = bits;
            out.push_back(anchor);
        }
    }
    return out;
}

RouteEngine::Ceiling RouteEngine::ceiling() {
    Ceiling out;
    out.per_chart.assign(lines_, kNone);

    std::array<Val, 64> acc;
    for (const Anchor& anchor : anchors()) {
        anchor_element_ = anchor.element;
        anchor_before_ = anchor.before;
        anchor_after_ = anchor.after;
        anchor_close_ = anchor.close;
        anchor_exit_col_ = element_index(anchor_before_, anchor_element_);
        if (anchor_exit_col_ < 0) continue;
        memo_.clear();
        values_.clear();

        const int anchor_entry = element_index(anchor_after_, anchor_element_);
        if (anchor_entry < 0) continue;
        const int slot = solve(anchor_after_, anchor_element_, anchor_entry,
                               anchor.bits);
        if (slot < 0) continue;

        for (int l = 0; l < lines_; ++l) acc[l] = 0;
        // Both faces of the anchor side are traversed as well.
        apply_loss(anchor_after_, acc.data());
        apply_loss(anchor_before_, acc.data());
        const Val* head = &values_[static_cast<std::size_t>(slot) * lines_];
        for (int l = 0; l < lines_; ++l) {
            const int total = acc[l] + head[l];
            if (out.per_chart[l] == kNone || total > out.per_chart[l]) {
                out.per_chart[l] = total;
            }
        }
        out.any_route = true;
    }

    for (int l = 0; l < lines_; ++l) {
        out.per_chart[l] = out.any_route ? base_[l] + out.per_chart[l] : kNone;
    }
    out.nodes = nodes_;
    return out;
}

}  // namespace addline
