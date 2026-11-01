// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Roman Parpalak and Denis Utkin

#pragma once

#include "omatrix.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

// Flat-buffer view of a closed OMatrix (no parallels, every row has length n-1).
// Stores n*(n-1) bytes row-major, byte values < n. Used in hot loops where
// allocating a new OMatrix per rotation would be wasteful (sym_detect, exact
// canonicalization). Buffer hash matches OMatrix::hash() byte-for-byte.
class FixedOMatrix {
public:
    static std::optional<FixedOMatrix> from_omatrix(const OMatrix& o);

    size_t n() const { return n_; }
    size_t key_len() const { return n_ == 0 ? 0 : n_ * (n_ - 1); }
    const uint8_t* data() const { return bytes_.data(); }
    const std::vector<uint8_t>& bytes() const { return bytes_; }

    FixedOMatrix() = default;

private:
    size_t n_ = 0;
    std::vector<uint8_t> bytes_;
};

namespace fixed_o {

// Rotate a closed flat buffer by rotation positions; write to out.
// out must point to n*(n-1) bytes. Always succeeds for closed input
// (no parallels => all rotations valid).
void rotate(const uint8_t* src, size_t n, size_t rotation, uint8_t* out);

// Mirror a closed flat buffer; write to out (also n*(n-1) bytes).
void mirror(const uint8_t* src, size_t n, uint8_t* out);

}  // namespace fixed_o
