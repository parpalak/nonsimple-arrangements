// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Roman Parpalak and Denis Utkin

#pragma once

#include <chrono>
#include <cstddef>
#include <ostream>
#include <string>

// Lightweight progress helpers shared by sorter, sym_detect and any
// future consumers in sorter_core.

namespace progress {

struct ProgressRate {
    std::size_t lines = 0;
    double seconds = 0.0;
    double lines_per_second = 0.0;
};

class ProgressTimer {
public:
    ProgressTimer();

    ProgressRate sample(std::size_t total_lines);
    bool is_due(double interval_sec) const;

private:
    std::chrono::steady_clock::time_point last_time_;
    std::size_t last_total_lines_ = 0;
};

std::string format_fixed(double value, int precision);

void print_progress_rate(std::ostream& os, const ProgressRate& rate);

}  // namespace progress
