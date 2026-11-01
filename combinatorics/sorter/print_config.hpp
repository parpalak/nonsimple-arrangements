// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Roman Parpalak and Denis Utkin

#pragma once

#include "configuration.hpp"
#include "params.hpp"

#include <optional>
#include <string>

struct StreamPrintOverrides {
    std::optional<std::string> eid;
    std::optional<std::string> pid;
};

// Returns the input string with letters uppercased (so "e" / "p" /
// "E" / "P" all normalize to the canonical "E" / "P" used elsewhere).
std::string normalize_uniq_mode(std::string uniq_mode);

// Apply --mirror / --rotate to conf.o (and refresh conf.gens). Returns
// false (and writes a "#INVALID ..." line) if the underlying o-matrix
// is missing or rotation fails.
bool apply_output_transformations(Configuration& conf, const Params& params);

// Dispatch over params.print tokens and write the configuration to
// std::cout. The overrides argument lets the exact-cache stream
// driver supply pre-computed eid / pid identifiers (which
// would otherwise require expensive recomputation through min_o /
// min_projective_o).
void print_configuration(Configuration& conf,
                         const Params& params,
                         const StreamPrintOverrides* overrides = nullptr);
