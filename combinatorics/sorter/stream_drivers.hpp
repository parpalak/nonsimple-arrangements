// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Roman Parpalak and Denis Utkin

#pragma once

#include "omatrix.hpp"
#include "params.hpp"
#include "state_io.hpp"

#include <cstddef>
#include <string>
#include <unordered_set>

// Plain streaming: read file line by line, parse each into a Configuration,
// apply --mirror / --rotate, print per --print. No deduplication.
int stream_print_file(const std::string& path,
                      const Params& params,
                      std::size_t& total_lines,
                      bool follow);

// Streaming with --uniq E or --uniq P: drop configurations whose canonical
// exact byte key is already in `key_set`. The generalized keys retain
// multiple-point boundaries and remain byte-compatible for simple words.
int stream_unique_file(const std::string& path,
                       const Params& params,
                       const std::string& uniq_mode,
                       std::unordered_set<std::string>& key_set,
                       std::size_t& total_lines,
                       bool follow);

// Streaming with --uniq P --exact-cache: uses compact byte-level keys
// (exact_key.hpp) and caches projective orbit E-classes in
// state.covered_e to skip O-matrix recomputation when an input lands
// in an already-seen P-class via a different affine chart.
int stream_unique_file_exact_cache(const std::string& path,
                                   const Params& params,
                                   ExactCacheState& state,
                                   std::size_t& resume_skip_remaining,
                                   bool count_resume_skipped_lines,
                                   bool follow);
