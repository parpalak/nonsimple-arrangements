// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Roman Parpalak and Denis Utkin

#pragma once

#include "exact_key.hpp"
#include "progress.hpp"

#include <cstddef>
#include <optional>
#include <string>

// Binary exact-cache state persistence (--save-state / --resume-state).
//
// On-disk format:
//   bytes  0..15   magic "SORTERSTATE00001"
//   bytes 16..23   u64 version (currently 1)
//   bytes 24..31   u64 expected_n
//   bytes 32..39   u64 total_lines
//   <covered_e arena>  three u64 (n, key_len, byte_size) + raw bytes
//   <seen_p arena>     same layout
//   <hit_bits>         u64 count + packed bits for covered_e
//
// No backwards compatibility with the older "ARNUTILSTATE0001" magic --
// that on-disk format was never used in production.

struct ExactCacheCounters {
    std::size_t total_lines = 0;
};

struct ExactCacheState {
    std::optional<std::size_t> expected_n;
    FixedLenExactKeyArena covered_e;
    FixedLenExactKeyArena seen_p;
    ExactCacheCounters counters;

    void ensure_arenas(std::size_t n);
};

// Returns 0 on success, -1 on error. Logs to stderr in both cases.
int save_exact_cache_state(const std::string& path, const ExactCacheState& state);
int load_exact_cache_state(const std::string& path, ExactCacheState& state);

// Progress line for the exact-cache stream driver. Writes to std::cerr.
void print_exact_cache_progress(const std::string& path,
                                std::size_t file_lines,
                                const ExactCacheState& state,
                                bool done,
                                const progress::ProgressRate& rate);
