// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Roman Parpalak and Denis Utkin

#pragma once

#include "symmetry_analyzer.hpp"

#include <string>

struct JsonWriterOptions {
    bool include_full_perms = false;
    bool include_gens = false;
    std::string gens_string;        // raw input generators line, used iff include_gens
};

// Serialize a SymmetryAnalysisResult to a single-line JSON string. The output
// has no trailing newline; the caller decides terminator and flushing policy.
std::string write_analysis_json(const SymmetryAnalysisResult& result,
                                const JsonWriterOptions& opts = {});

// Internal helper: escape a string for JSON. Exposed for tests.
std::string json_escape_string(const std::string& s);
