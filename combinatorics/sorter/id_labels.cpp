// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Roman Parpalak and Denis Utkin

#include "id_labels.hpp"

#include <algorithm>

const std::string B36_DIGITS = "0123456789abcdefghijklmnopqrstuvwxyz";

std::string base36_encode(uint64_t value) {
    if (value == 0)
        return "0";

    std::string result = "";
    while (value > 0) {
        result += B36_DIGITS[value % 36];
        value /= 36;
    }

    std::reverse(result.begin(), result.end());
    return result;
}

std::string eid_label(size_t n, uint64_t hash) {
    return "E" + std::to_string(n) + "-" + base36_encode(hash);
}

std::string pid_label(size_t n, uint64_t hash) {
    return "P" + std::to_string(n) + "-" + base36_encode(hash);
}
