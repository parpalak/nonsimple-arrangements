// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Roman Parpalak and Denis Utkin

#pragma once

#include "omatrix.hpp"

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

// Permutation of line labels in the form oldToNew, fixed-size (n+1 for an
// OMatrix of n finite lines, with index n reserved for the line at
// infinity). Convention: (g.compose(h))(x) = h(g(x)), g is applied
// first, then h.
class LinePermutation {
public:
    LinePermutation() = default;
    explicit LinePermutation(std::vector<size_t> old_to_new);

    static LinePermutation identity(size_t size);

    size_t size() const { return old_to_new_.size(); }
    size_t image(size_t old_line) const;
    const std::vector<size_t>& as_array() const { return old_to_new_; }

    LinePermutation inverse() const;
    LinePermutation compose(const LinePermutation& after) const;
    size_t order() const;
    std::string to_cycles_string() const;

    bool operator==(const LinePermutation& other) const {
        return old_to_new_ == other.old_to_new_;
    }
    bool operator!=(const LinePermutation& other) const { return !(*this == other); }
    bool operator<(const LinePermutation& other) const {
        return old_to_new_ < other.old_to_new_;
    }

private:
    std::vector<size_t> old_to_new_;
};

// Pair of OMatrix and LinePermutation tracking the mapping of original line
// labels to current line labels. The permutation has size matrix.n() + 1.
struct TracedOMatrix {
    OMatrix matrix;
    LinePermutation old_to_new_line;
};

TracedOMatrix traced_identity(const OMatrix& matrix);
TracedOMatrix traced_projective_rotation(const TracedOMatrix& traced, size_t p);
std::optional<TracedOMatrix> traced_rotate(const TracedOMatrix& traced, size_t rotation);
TracedOMatrix traced_reflect(const TracedOMatrix& traced);
