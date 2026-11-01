// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Roman Parpalak and Denis Utkin

#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "closure.hpp"

namespace addline {

// The mask of crossed lines alone takes up to 64 bits, so the face and the
// entry element go into a word of their own.
struct MemoKey {
    std::uint64_t site = 0;
    std::uint64_t crossed = 0;

    bool operator==(const MemoKey& other) const {
        return site == other.site && crossed == other.crossed;
    }
};

// Open addressing instead of unordered_map: the memo is probed once per move
// and wiped once per anchor, so entries carry an epoch and clear() is a bump.
template <class V>
class FlatMap {
public:
    void init(std::size_t want) {
        std::size_t cap = 1;
        while (cap < want) cap <<= 1;
        allocate(cap);
    }

    void clear() {
        size_ = 0;
        if (++epoch_ != 0) return;
        table_.assign(table_.size(), Entry{});  // wrapped: drop every epoch
        epoch_ = 1;
    }

    const V* find(const MemoKey& key) const {
        for (std::size_t i = start(key);; i = (i + 1) & mask_) {
            const Entry& e = table_[i];
            if (e.epoch != epoch_) return nullptr;
            if (e.key == key) return &e.value;
        }
    }

    // KEY must be absent.
    void insert(const MemoKey& key, const V& value) {
        if (2 * (size_ + 1) > table_.size()) grow();
        ++size_;
        place(key, value);
    }

private:
    struct Entry {
        MemoKey key;
        std::uint32_t epoch = 0;
        V value{};
    };

    std::size_t start(const MemoKey& key) const {
        const std::uint64_t mixed = key.crossed ^ (key.site * 0xC2B2AE3D27D4EB4Full);
        return (mixed * 0x9E3779B97F4A7C15ull) >> shift_;  // Fibonacci, high bits
    }

    void allocate(std::size_t cap) {
        table_.assign(cap, Entry{});
        mask_ = cap - 1;
        shift_ = 64;
        while (cap > 1) {
            --shift_;
            cap >>= 1;
        }
        epoch_ = 1;
        size_ = 0;
    }

    void place(const MemoKey& key, const V& value) {
        std::size_t i = start(key);
        while (table_[i].epoch == epoch_) i = (i + 1) & mask_;
        table_[i] = Entry{key, epoch_, value};
    }

    void grow() {
        std::vector<Entry> live;
        live.reserve(size_);
        for (const Entry& e : table_) {
            if (e.epoch == epoch_) live.push_back(e);
        }
        allocate(table_.size() * 2);
        size_ = live.size();
        for (const Entry& e : live) place(e.key, e.value);
    }

    std::vector<Entry> table_;
    std::size_t mask_ = 0;
    unsigned shift_ = 64;
    std::uint32_t epoch_ = 1;
    std::size_t size_ = 0;
};

// Routes of the new line through the faces of the closure. A route leaves a
// face through a side or a simple vertex (which becomes a triple point); stars
// are impassable. Values are per chart: entry l counts the triangles missing
// line l, i.e. the finite triangles of the chart with l at infinity.
class RouteEngine {
public:
    explicit RouteEngine(const Closure& closure);

    // Upper bound: without the no-revisit rule the position graph is a DAG.
    struct Ceiling {
        std::vector<int> per_chart;  // triangles of the whole n+1 arrangement
        long nodes = 0;
        bool any_route = false;
        int best_chart() const;
    };

    Ceiling ceiling();

    // Anchors on FACE instead of the line at infinity (2n elements against a
    // handful, one DP per anchor). Routes avoiding FACE are not searched; -1
    // restores the line at infinity. SIDES_ONLY also drops corner entries.
    void anchor_on(int face, bool sides_only = false) {
        anchor_face_ = face;
        anchor_sides_only_ = sides_only;
        chords_.clear();
    }

    // Anchors on the chords that cut a non-triangular sector of STAR into a
    // triangle -- the move the template forces on the deficient star.
    void anchor_on_star_cuts(int star);

    struct Step {
        int face = -1;
        int element = -1;
    };

    struct Witness {
        // A side or a crossing of the line at infinity; a crossing has no
        // word in this chart.
        int anchor_element = -1;
        std::vector<Step> steps;  // in traversal order, closing step included
        std::vector<int> per_chart;
        int chart = -1;           // a chart attaining the target
        int triangles = 0;        // its finite triangle count
    };

    // Exact search, pruned by the ceiling DP against TARGET.
    struct SearchResult {
        std::vector<Witness> witnesses;
        long states = 0;
        long dp_nodes = 0;
    };

    SearchResult search(int target, bool all_witnesses);

    const std::vector<int>& core_per_chart() const { return base_; }

private:
    // Element codes: even for a side, odd for a simple vertex.
    static int side_element(int key) { return 2 * key; }
    static int vertex_element(int event) { return 2 * event + 1; }
    static bool is_vertex(int element) { return element % 2 == 1; }
    static int element_value(int element) { return element / 2; }

    struct Move {
        int to_face;
        int element;
        // Local numbers of ELEMENT in the two faces; index the cut table.
        int col = -1;
        int to_index = -1;
        std::uint64_t bits;
    };

    struct FaceGeometry {
        int m = 0;
        std::vector<int> dups;          // vertices appearing twice on the rim
        bool is_triangle = false;
        std::uint64_t triangle_lines = 0;  // lines this triangle meets
    };

    // Triangles one chord cuts off inside a face (at most two).
    struct Cut {
        int pieces = 0;
        std::uint64_t lines[2] = {0, 0};
    };

    // A chart holds a few hundred triangles at most; int16 doubles the SIMD
    // lanes.
    using Val = std::int16_t;

    int position_of(int face, int element) const;
    Cut compute_cut(int face, int entry, int exit) const;
    // SCRATCH: an element with no local number, possible only for a face
    // glued to itself along the anchor.
    const Cut& cut(int face, int entry, int entry_index, int exit, int col,
                   Cut& scratch);
    int element_index(int face, int element) const;
    void apply_loss(int face, Val* values) const;

    int solve(int face, int element, int entry_index, std::uint64_t crossed);
    void walk(int face, int element, int entry_index, std::uint64_t crossed,
              const Val* acc, SearchResult& out);

    const Closure& c_;
    int lines_ = 0;
    std::uint64_t full_ = 0;
    std::vector<int> base_;
    std::vector<FaceGeometry> geometry_;
    std::vector<std::vector<Move>> moves_;

    // One dense row per (face, entry element), filled on first use; direct
    // indexing beats hashing at these sizes.
    struct CutRows {
        int base = 0;  // where the face's block starts in CUT_TABLE_
        int k = 0;     // its side, the number of elements of the face
    };
    std::vector<std::vector<int>> face_elems_;
    std::vector<CutRows> cut_rows_;
    std::vector<Cut> cut_table_;

    // memo of the relaxed DP: state key -> slot in values_, or -1 for a
    // position no route can close from
    FlatMap<int> memo_;
    std::vector<Val> values_;
    long nodes_ = 0;
    struct Anchor {
        int element = -1;
        int before = -1;
        int after = -1;
        std::uint64_t bits = 0;  // lines the crossing itself consumes
        // Element to close BEFORE through, -1 for any (set for chords).
        int close = -1;
    };
    std::vector<Anchor> anchors() const;

    int anchor_face_ = -1;
    bool anchor_sides_only_ = false;
    struct Chord {
        int face = -1;
        int side_a = -1, side_b = -1;  // edge keys
    };
    std::vector<Chord> chords_;
    int anchor_before_ = -1, anchor_after_ = -1, anchor_element_ = -1;
    int anchor_close_ = -1;
    int anchor_exit_col_ = -1;  // local number of the anchor element in ANCHOR_BEFORE_

    // exact search state
    int target_ = 0;
    bool all_witnesses_ = false;
    std::vector<char> visited_;
    std::vector<Step> path_;
};

}  // namespace addline
