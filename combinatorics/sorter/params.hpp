// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Roman Parpalak and Denis Utkin

#pragma once

#include <cstddef>
#include <string>
#include <vector>

struct Params {
    std::string ifile;
    std::vector<std::string> ifiles;
    std::size_t line_num = 0;
    std::size_t rotate = 0;
    double progress = 0;
    std::size_t resume_skip_lines = 0;
    bool mirror = false;
    bool follow = false;
    bool exact_cache = false;
    std::string resume_output;
    std::string resume_state;
    std::string save_state;
    std::size_t state_progress = 0;
    std::string uniq_mode;
    std::vector<std::string> print;
};
