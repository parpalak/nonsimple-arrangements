// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Roman Parpalak and Denis Utkin

#include "progress.hpp"

#include <iomanip>
#include <sstream>

namespace progress {

ProgressTimer::ProgressTimer()
    : last_time_(std::chrono::steady_clock::now()) {}

ProgressRate ProgressTimer::sample(std::size_t total_lines) {
    auto now = std::chrono::steady_clock::now();
    double seconds = std::chrono::duration<double>(now - last_time_).count();
    std::size_t lines = total_lines - last_total_lines_;
    last_time_ = now;
    last_total_lines_ = total_lines;

    ProgressRate rate;
    rate.lines = lines;
    rate.seconds = seconds;
    if (seconds > 0.0)
        rate.lines_per_second = static_cast<double>(lines) / seconds;
    return rate;
}

bool ProgressTimer::is_due(double interval_sec) const {
    auto now = std::chrono::steady_clock::now();
    double elapsed = std::chrono::duration<double>(now - last_time_).count();
    return elapsed >= interval_sec;
}

std::string format_fixed(double value, int precision) {
    std::ostringstream out;
    out << std::fixed << std::setprecision(precision) << value;
    return out.str();
}

void print_progress_rate(std::ostream& os, const ProgressRate& rate) {
    if (rate.lines == 0)
        return;
    os << " interval_lines=" << rate.lines
       << " rate_lps=" << format_fixed(rate.lines_per_second, 1);
}

}  // namespace progress
