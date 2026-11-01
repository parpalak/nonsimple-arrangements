// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Roman Parpalak and Denis Utkin

#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace addline {

// The wires at positions pos .. pos+k-1 meet at one point and reverse.
struct Step {
    int pos = 0;
    int k = 2;

    bool operator==(const Step& other) const {
        return pos == other.pos && k == other.k;
    }
};

using Word = std::vector<Step>;

struct ParseResult {
    bool ok = false;
    Word word;
    std::string reason;
};

// Anything up to a ')' or a '|' is metadata. A point of multiplicity k is
// written with k-2 stars: "13", "13*", "13**".
ParseResult parse_word(const std::string& line);

std::string format_word(const Word& word);

inline std::size_t simple_length(int n) {
    return static_cast<std::size_t>(n) * static_cast<std::size_t>(n - 1) / 2;
}

}  // namespace addline
