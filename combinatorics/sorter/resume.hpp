// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Roman Parpalak and Denis Utkin

#pragma once

#include "state_io.hpp"

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

struct ResumeLineInfo {
    std::vector<std::size_t> gens;
    bool parse_ok = false;
    std::string parse_error;
    std::optional<std::string> expected_pid;
    std::optional<std::string> expected_eid;
};

ResumeLineInfo parse_resume_line(const std::string& line);

// Replay a previous exact-cache stream output file: parse each emitted
// configuration, build its projective orbit, and prime `state.covered_e`
// / `state.seen_p` so the live stream picks up where the previous run
// left off without recomputing already-seen P-classes.
//
// `skip_lines_for_logging` is only used in the [resume] log header; it
// does not change behavior. `progress_interval_sec` controls the
// throttle for [resume] progress lines (0 = disabled).
//
// Returns 0 on success, -1 on error. Writes diagnostics to stderr.
int load_exact_cache_resume_output(const std::string& path,
                                   std::size_t skip_lines_for_logging,
                                   double progress_interval_sec,
                                   ExactCacheState& state);
