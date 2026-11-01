// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Roman Parpalak and Denis Utkin

#include "fixed_omatrix.hpp"

#include <cstring>

std::optional<FixedOMatrix> FixedOMatrix::from_omatrix(const OMatrix& o) {
    if (o.n() == 0 || !o.parallels.empty())
        return std::nullopt;

    FixedOMatrix r;
    r.n_ = o.n();
    const size_t row_len = r.n_ - 1;
    r.bytes_.resize(r.n_ * row_len);

    uint8_t* dst = r.bytes_.data();
    for (const auto& line : o.intersections) {
        if (line.size() != row_len)
            return std::nullopt;
        for (size_t v : line) {
            if (v >= r.n_)
                return std::nullopt;
            *dst++ = static_cast<uint8_t>(v);
        }
    }
    return r;
}

namespace fixed_o {

void rotate(const uint8_t* src, size_t n, size_t rotation, uint8_t* out) {
    const size_t row_len = n - 1;
    rotation %= 2 * n;

    if (rotation == 0) {
        std::memcpy(out, src, n * row_len);
        return;
    }

    // Mirrors OMatrix::get_val for closed matrices (no parallels).
    //   l = (i + rotation) mod 2n
    //   if l < n:  src_row = l,         src_col = j
    //   else:      src_row = l - n,     src_col = (n-2) - j
    //   v = src[src_row * row_len + src_col]
    //   out[i*row_len + j] = (v + 2n - rotation) mod n  (precomputed via lut[])
    const size_t two_n = 2 * n;
    uint8_t lut[256];
    for (size_t v = 0; v < n; ++v)
        lut[v] = static_cast<uint8_t>((v + two_n - rotation) % n);

    for (size_t i = 0; i < n; ++i) {
        size_t l = i + rotation;
        if (l >= two_n) l -= two_n;
        const uint8_t* src_row;
        bool reversed;
        if (l < n) {
            src_row = src + l * row_len;
            reversed = false;
        } else {
            src_row = src + (l - n) * row_len;
            reversed = true;
        }
        uint8_t* dst = out + i * row_len;
        if (!reversed) {
            for (size_t j = 0; j < row_len; ++j)
                dst[j] = lut[src_row[j]];
        } else {
            for (size_t j = 0; j < row_len; ++j)
                dst[j] = lut[src_row[row_len - 1 - j]];
        }
    }
}

void mirror(const uint8_t* src, size_t n, uint8_t* out) {
    const size_t row_len = n - 1;
    // out[k][j] = (n-1) - src[(n-1-k)][j]
    for (size_t k = 0; k < n; ++k) {
        const uint8_t* src_row = src + (n - 1 - k) * row_len;
        uint8_t* dst = out + k * row_len;
        const uint8_t base = static_cast<uint8_t>(n - 1);
        for (size_t j = 0; j < row_len; ++j) {
            dst[j] = static_cast<uint8_t>(base - src_row[j]);
        }
    }
}

}  // namespace fixed_o
