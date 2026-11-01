// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Roman Parpalak and Denis Utkin

#include "exact_key.hpp"

#include "fixed_omatrix.hpp"

#include <algorithm>
#include <cstring>
#include <limits>
#include <numeric>
#include <stdexcept>

uint64_t hash_bytes(const uint8_t* bytes, size_t len) {
    uint64_t h = 0xcbf29ce484222325ULL;
    const uint64_t p = 0x100000001b3ULL;

    for (size_t i = 0; i < len; ++i) {
        uint8_t v = bytes[i];
        h ^= static_cast<uint64_t>(v);
        h *= p;
    }

    return h;
}

uint64_t hash_bytes(const std::vector<uint8_t>& bytes) {
    return hash_bytes(bytes.data(), bytes.size());
}

std::optional<CompactKey> compact_key(const OMatrix& o, size_t expected_n) {
    if (expected_n == 0 || expected_n > 255)
        return {};

    if (o.n() != expected_n || !o.parallels.empty())
        return {};

    const size_t key_len = expected_n * (expected_n - 1);
    CompactKey key;
    key.n = expected_n;
    key.bytes.reserve(key_len);

    for (auto &line : o.intersections) {
        if (line.size() != expected_n - 1)
            return {};

        for (size_t v : line) {
            if (v >= expected_n)
                return {};
            key.bytes.push_back(static_cast<uint8_t>(v));
        }
    }

    if (key.bytes.size() != key_len)
        return {};

    key.hash = hash_bytes(key.bytes);
    return key;
}

bool compact_key_less(const CompactKey& lhs, const CompactKey& rhs) {
    return std::lexicographical_compare(lhs.bytes.begin(), lhs.bytes.end(),
                                        rhs.bytes.begin(), rhs.bytes.end());
}

std::optional<ExactMinOKey> exact_min_o_key(const OMatrix& o, size_t expected_n) {
    // Closed-only fast path: flatten into a byte buffer once, then rotate/mirror
    // into reusable scratch buffers (no per-iteration heap allocation).
    auto flat = FixedOMatrix::from_omatrix(o);
    if (!flat.has_value())
        return {};
    if (flat->n() != expected_n)
        return {};

    const size_t key_len = flat->key_len();

    ExactMinOKey result;
    result.key.n = expected_n;
    result.key.bytes.assign(flat->bytes().begin(), flat->bytes().end());

    std::vector<uint8_t> scratch(key_len);

    // Track the lexicographically minimal byte buffer across the orbit; its
    // hash is the class identifier (arXiv:2607.29236, Definition D.1). The hash is
    // computed once at the end from the winning buffer, not per iteration.
    auto consider = [&](const uint8_t* buf) {
        if (std::memcmp(buf, result.key.bytes.data(), key_len) < 0)
            std::memcpy(result.key.bytes.data(), buf, key_len);
    };

    for (size_t i = 1; i < 2 * flat->n(); ++i) {
        fixed_o::rotate(flat->data(), flat->n(), i, scratch.data());
        consider(scratch.data());
    }

    std::vector<uint8_t> mirrored(key_len);
    fixed_o::mirror(flat->data(), flat->n(), mirrored.data());
    consider(mirrored.data());
    for (size_t i = 1; i < 2 * flat->n(); ++i) {
        fixed_o::rotate(mirrored.data(), flat->n(), i, scratch.data());
        consider(scratch.data());
    }

    result.key.hash = hash_bytes(result.key.bytes.data(), key_len);
    return result;
}

std::optional<ProjectiveOrbitKeys> get_projective_orbit_keys(const OMatrix& o, size_t expected_n, ExactMinOKey input_e) {
    ProjectiveOrbitKeys result;
    result.e_keys.push_back(std::move(input_e.key));
    result.p_key = result.e_keys.back();

    for (size_t i = 0; i < o.n(); ++i) {
        auto proj = o.projective_rotate(i);
        if (!proj.has_value())
            continue;

        auto e = exact_min_o_key(*proj, expected_n);
        if (!e.has_value())
            return {};

        if (compact_key_less(e->key, result.p_key))
            result.p_key = e->key;
        result.e_keys.push_back(std::move(e->key));
    }

    return result;
}

std::optional<ProjectiveOrbitKeys> get_projective_orbit_keys(const OMatrix& o, size_t expected_n) {
    auto input_e = exact_min_o_key(o, expected_n);
    if (!input_e.has_value())
        return {};

    return get_projective_orbit_keys(o, expected_n, std::move(*input_e));
}

void FixedLenExactKeyArena::init(size_t new_n) {
    n = new_n;
    key_len = n * (n - 1);
}

bool FixedLenExactKeyArena::is_initialized() const {
    return n != 0;
}

size_t FixedLenExactKeyArena::size() const {
    return key_len == 0 ? 0 : bytes.size() / key_len;
}

void FixedLenExactKeyArena::rebuild_index() {
    index.clear();
    if (key_len == 0)
        return;

    size_t count = size();
    for (size_t id = 0; id < count; ++id) {
        if (id > std::numeric_limits<uint32_t>::max())
            throw std::runtime_error("too many exact keys for uint32 ids");
        const uint8_t* key_bytes = bytes.data() + id * key_len;
        uint64_t h = hash_bytes(key_bytes, key_len);
        index[h].push_back(static_cast<uint32_t>(id));
    }
}

bool FixedLenExactKeyArena::same_layout(const CompactKey& key) const {
    return key.n == n && key.bytes.size() == key_len;
}

bool FixedLenExactKeyArena::contains(const CompactKey& key) const {
    if (!same_layout(key))
        return false;

    auto it = index.find(key.hash);
    if (it == index.end())
        return false;

    for (uint32_t id : it->second) {
        const uint8_t* candidate = bytes.data() + static_cast<size_t>(id) * key_len;
        if (std::memcmp(candidate, key.bytes.data(), key_len) == 0)
            return true;
    }

    return false;
}

bool FixedLenExactKeyArena::insert_if_absent(const CompactKey& key) {
    if (!same_layout(key))
        throw std::runtime_error("exact key arena layout mismatch");

    auto &bucket = index[key.hash];
    for (uint32_t id : bucket) {
        const uint8_t* candidate = bytes.data() + static_cast<size_t>(id) * key_len;
        if (std::memcmp(candidate, key.bytes.data(), key_len) == 0)
            return false;
    }

    size_t id = size();
    if (id > std::numeric_limits<uint32_t>::max())
        throw std::runtime_error("too many exact keys for uint32 ids");

    bytes.insert(bytes.end(), key.bytes.begin(), key.bytes.end());
    bucket.push_back(static_cast<uint32_t>(id));
    return true;
}

bool FixedLenExactKeyArena::contains_and_mark(const CompactKey& key) {
    if (!same_layout(key))
        return false;

    auto it = index.find(key.hash);
    if (it == index.end())
        return false;

    for (uint32_t id : it->second) {
        const uint8_t* candidate = bytes.data() + static_cast<size_t>(id) * key_len;
        if (std::memcmp(candidate, key.bytes.data(), key_len) == 0) {
            size_t idx = static_cast<size_t>(id);
            if (idx >= hit.size())
                hit.resize(size(), false);
            if (!hit[idx]) {
                hit[idx] = true;
                ++hit_count_;
            }
            return true;
        }
    }

    return false;
}

size_t FixedLenExactKeyArena::hit_count() const {
    return hit_count_;
}

std::optional<OMatrix> get_exact_cache_OMatrix(const std::vector<size_t>& gens, std::string& reason) {
    if (gens.empty()) {
        reason = "empty generator list";
        return {};
    }

    size_t n = *std::max_element(gens.begin(), gens.end()) + 2;
    const size_t expected_length = n * (n - 1) / 2;
    if (gens.size() != expected_length) {
        reason = "exact cache supports only n(n-1)/2 generators; got n="
               + std::to_string(n) + " gens=" + std::to_string(gens.size());
        return {};
    }
    if (n > 255) {
        reason = "exact cache supports uint8 keys only for n <= 255; got n=" + std::to_string(n);
        return {};
    }

    std::vector<size_t> lines(n);
    std::iota(lines.begin(), lines.end(), 0);
    std::vector<std::vector<size_t>> intersections(n);

    for (size_t step = 0; step < gens.size(); ++step)
    {
        size_t gen = gens[step];
        if (gen + 1 >= n) {
            reason = "generator " + std::to_string(gen) + " is out of range 0.." + std::to_string(n - 2);
            return {};
        }

        size_t first = lines[gen];
        size_t second = lines[gen + 1];
        if (first > second) {
            reason = "generator at step " + std::to_string(step + 1)
                   + " removes an existing inversion";
            return {};
        }

        intersections[first].push_back(second);
        intersections[second].push_back(first);

        lines[gen] = second;
        lines[gen + 1] = first;
    }

    OMatrix o{intersections, {}};
    for (auto &line : o.intersections) {
        if (line.size() != n - 1) {
            reason = "exact cache requires full fixed layout; got a line with "
                   + std::to_string(line.size()) + " intersections for n=" + std::to_string(n);
            return {};
        }
    }

    return o;
}

bool validate_exact_cache_input_n(size_t n, std::optional<size_t>& expected_n, std::string& reason) {
    if (!expected_n.has_value()) {
        expected_n = n;
        return true;
    }

    if (*expected_n != n) {
        reason = "mixed n in exact-cache stream; expected n=" + std::to_string(*expected_n)
               + " got n=" + std::to_string(n);
        return false;
    }

    return true;
}
