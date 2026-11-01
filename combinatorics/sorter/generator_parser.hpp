// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Roman Parpalak and Denis Utkin

#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "word.hpp"

struct GeneratorParseResult {
    bool ok = false;
    bool expanded_even_generators = false;
    bool has_multiple_points = false;
    std::size_t n = 0;
    std::vector<size_t> gens;
    addline::Word word;
    std::string reason;
};

GeneratorParseResult parse_generators(const std::string& s);

std::vector<size_t> gens_str_to_vec(const std::string& s);
