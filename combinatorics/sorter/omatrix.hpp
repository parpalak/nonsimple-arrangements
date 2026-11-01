// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Roman Parpalak and Denis Utkin

#pragma once

#include <vector>
#include <map>
#include <optional>
#include <cstddef>
#include <cstdint>
#include <functional>

struct OMatrix {
    std::vector<std::vector<size_t>> intersections;
    std::map<size_t, size_t> parallels;
    mutable std::optional<uint64_t> cached_hash;

    size_t n() const;
    size_t norm(size_t l) const;
    size_t norm2(size_t l) const;
    size_t get_parallels_ind(size_t l) const;
    size_t get_val(size_t rotation, size_t i, size_t j) const;
    bool can_rotate(size_t rotation) const;
    size_t get_line_len(size_t rotation, size_t i) const;
    size_t rotate_line_num(size_t rotation, size_t l) const;
    std::optional<OMatrix> rotate(size_t rotation) const;
    size_t get_num_vertices() const;
    std::vector<size_t> get_generators() const;
    OMatrix mirror() const;
    std::optional<OMatrix> projective_rotate(size_t val) const;
    uint64_t hash() const;
    OMatrix min_o() const;
    std::optional<OMatrix> min_projective_o() const;
    bool operator==(const OMatrix& other) const;
};

namespace std {
    template<>
    struct hash<OMatrix> {
        std::size_t operator()(const OMatrix& o) const noexcept {
            return o.hash();
        }
    };
}

std::optional<OMatrix> get_OMatrix(std::vector<size_t> gens);
