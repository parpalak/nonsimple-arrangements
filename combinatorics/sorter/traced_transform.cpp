// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Roman Parpalak and Denis Utkin

#include "traced_transform.hpp"

#include <numeric>
#include <sstream>
#include <stdexcept>

namespace {

size_t gcd_size(size_t a, size_t b) {
    while (b != 0) {
        size_t t = a % b;
        a = b;
        b = t;
    }
    return a;
}

size_t lcm_size(size_t a, size_t b) {
    if (a == 0 || b == 0)
        return 0;
    return (a / gcd_size(a, b)) * b;
}

}  // namespace

LinePermutation::LinePermutation(std::vector<size_t> old_to_new)
    : old_to_new_(std::move(old_to_new)) {
    const size_t n = old_to_new_.size();
    if (n == 0)
        throw std::invalid_argument("LinePermutation requires positive size");

    std::vector<bool> seen(n, false);
    for (size_t v : old_to_new_) {
        if (v >= n)
            throw std::invalid_argument("LinePermutation value out of range");
        if (seen[v])
            throw std::invalid_argument("LinePermutation values must be unique");
        seen[v] = true;
    }
}

LinePermutation LinePermutation::identity(size_t size) {
    if (size == 0)
        throw std::invalid_argument("LinePermutation::identity requires positive size");
    std::vector<size_t> v(size);
    std::iota(v.begin(), v.end(), size_t{0});
    return LinePermutation(std::move(v));
}

size_t LinePermutation::image(size_t old_line) const {
    if (old_line >= old_to_new_.size())
        throw std::out_of_range("LinePermutation::image out of range");
    return old_to_new_[old_line];
}

LinePermutation LinePermutation::inverse() const {
    std::vector<size_t> inv(old_to_new_.size(), 0);
    for (size_t i = 0; i < old_to_new_.size(); ++i) {
        inv[old_to_new_[i]] = i;
    }
    return LinePermutation(std::move(inv));
}

LinePermutation LinePermutation::compose(const LinePermutation& after) const {
    if (size() != after.size())
        throw std::invalid_argument("Cannot compose line permutations of different sizes");
    std::vector<size_t> r;
    r.reserve(size());
    for (size_t middle : old_to_new_) {
        r.push_back(after.old_to_new_[middle]);
    }
    return LinePermutation(std::move(r));
}

size_t LinePermutation::order() const {
    const size_t n = old_to_new_.size();
    std::vector<bool> seen(n, false);
    size_t order = 1;
    for (size_t start = 0; start < n; ++start) {
        if (seen[start])
            continue;
        size_t length = 0;
        size_t line = start;
        do {
            seen[line] = true;
            ++length;
            line = old_to_new_[line];
        } while (!seen[line]);
        if (length > 1)
            order = lcm_size(order, length);
    }
    return order;
}

std::string LinePermutation::to_cycles_string() const {
    const size_t n = old_to_new_.size();
    std::vector<bool> seen(n, false);
    std::ostringstream out;
    bool any = false;
    for (size_t start = 0; start < n; ++start) {
        if (seen[start])
            continue;
        std::vector<size_t> cycle;
        size_t line = start;
        do {
            seen[line] = true;
            cycle.push_back(line);
            line = old_to_new_[line];
        } while (!seen[line]);
        if (cycle.size() > 1) {
            out << '(';
            for (size_t i = 0; i < cycle.size(); ++i) {
                if (i > 0)
                    out << ' ';
                out << cycle[i];
            }
            out << ')';
            any = true;
        }
    }
    if (!any)
        return "()";
    return out.str();
}

TracedOMatrix traced_identity(const OMatrix& matrix) {
    return TracedOMatrix{matrix, LinePermutation::identity(matrix.n() + 1)};
}

TracedOMatrix traced_projective_rotation(const TracedOMatrix& traced, size_t p) {
    const OMatrix& m = traced.matrix;
    const size_t n = m.n();
    if (p >= n)
        throw std::out_of_range("traced_projective_rotation: p out of range");

    auto rotated = m.projective_rotate(p);
    if (!rotated.has_value())
        throw std::runtime_error("traced_projective_rotation: projective_rotate failed");

    // Build the relabeling tau induced by pi_p: line p becomes the new
    // line at infinity (index n), the previous line at infinity becomes
    // finite at position n-1, and each other line k is reindexed by its
    // position in row p of the original O-matrix (i.e., the order in
    // which line p meets it). This is the row-level relabeling of
    // pi_p (arXiv:2607.29236, Definition 5.6, formula (C.5)).
    std::vector<size_t> sentinel(n + 1, n + 1);  // marker for "unfilled"
    const auto& row = m.intersections[p];
    for (size_t new_line = 0; new_line < row.size(); ++new_line) {
        size_t old_line = row[new_line];
        sentinel[old_line] = new_line;
    }
    sentinel[n] = (n == 0) ? 0 : n - 1;
    sentinel[p] = n;

    // Validate that all entries got assigned.
    std::vector<size_t> old_to_new(n + 1);
    for (size_t i = 0; i <= n; ++i) {
        if (sentinel[i] > n)
            throw std::runtime_error("traced_projective_rotation: incomplete row");
        old_to_new[i] = sentinel[i];
    }

    LinePermutation step(std::move(old_to_new));
    return TracedOMatrix{*rotated, traced.old_to_new_line.compose(step)};
}

std::optional<TracedOMatrix> traced_rotate(const TracedOMatrix& traced, size_t rotation) {
    const OMatrix& m = traced.matrix;
    auto rotated = m.rotate(rotation);
    if (!rotated.has_value())
        return std::nullopt;

    const size_t n = m.n();
    size_t r = rotation % (2 * n);

    std::vector<size_t> old_to_new(n + 1);
    for (size_t line = 0; line < n; ++line) {
        old_to_new[line] = m.rotate_line_num(r, line);
    }
    old_to_new[n] = n;  // projective infinity is fixed under euclidean rotation

    LinePermutation step(std::move(old_to_new));
    return TracedOMatrix{*rotated, traced.old_to_new_line.compose(step)};
}

TracedOMatrix traced_reflect(const TracedOMatrix& traced) {
    const OMatrix& m = traced.matrix;
    const size_t n = m.n();

    std::vector<size_t> old_to_new(n + 1);
    for (size_t line = 0; line < n; ++line) {
        old_to_new[line] = n - 1 - line;
    }
    old_to_new[n] = n;

    LinePermutation step(std::move(old_to_new));
    return TracedOMatrix{m.mirror(), traced.old_to_new_line.compose(step)};
}
