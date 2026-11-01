// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Roman Parpalak and Denis Utkin

#pragma once

#include "omatrix.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

struct CompactKey {
    size_t n = 0;
    std::vector<uint8_t> bytes;
    uint64_t hash = 0;
};

struct ExactMinOKey {
    CompactKey key;
};

struct ProjectiveOrbitKeys {
    CompactKey p_key;
    std::vector<CompactKey> e_keys;
};

uint64_t hash_bytes(const uint8_t* bytes, size_t len);
uint64_t hash_bytes(const std::vector<uint8_t>& bytes);

std::optional<CompactKey> compact_key(const OMatrix& o, size_t expected_n);
bool compact_key_less(const CompactKey& lhs, const CompactKey& rhs);

std::optional<ExactMinOKey> exact_min_o_key(const OMatrix& o, size_t expected_n);

std::optional<ProjectiveOrbitKeys> get_projective_orbit_keys(const OMatrix& o, size_t expected_n, ExactMinOKey input_e);
std::optional<ProjectiveOrbitKeys> get_projective_orbit_keys(const OMatrix& o, size_t expected_n);

struct FixedLenExactKeyArena {
    size_t n = 0;
    size_t key_len = 0;
    std::vector<uint8_t> bytes;
    std::unordered_map<uint64_t, std::vector<uint32_t>> index;
    std::vector<bool> hit;
    size_t hit_count_ = 0;

    void init(size_t new_n);
    bool is_initialized() const;
    size_t size() const;
    void rebuild_index();
    bool same_layout(const CompactKey& key) const;
    bool contains(const CompactKey& key) const;
    bool insert_if_absent(const CompactKey& key);
    bool contains_and_mark(const CompactKey& key);
    size_t hit_count() const;
};

std::optional<OMatrix> get_exact_cache_OMatrix(const std::vector<size_t>& gens, std::string& reason);
bool validate_exact_cache_input_n(size_t n, std::optional<size_t>& expected_n, std::string& reason);
