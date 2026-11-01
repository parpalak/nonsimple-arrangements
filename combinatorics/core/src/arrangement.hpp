// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Roman Parpalak and Denis Utkin

#pragma once

#include <string>
#include <vector>

#include "word.hpp"

namespace addline {

struct Events {
    int n = 0;
    std::vector<std::vector<int>> at;  // at[event]  = lines, in wire order
    std::vector<std::vector<int>> on;  // on[line]   = events, in sweep order
};

struct ReplayResult {
    bool ok = false;
    Events events;
    // Lines the word never crosses, ascending, one list per line.
    std::vector<std::vector<int>> parallel;
    std::string reason;
};

// INITIAL defaults to 0..n-1. A pair that meets twice is refused; a pair that
// never meets is reported as parallel.
ReplayResult replay(const Word& word, int n);
ReplayResult replay(const Word& word, int n, const std::vector<int>& initial);

// A parallel class: consecutive labels FIRST .. FIRST+SIZE-1.
struct ParallelBlock {
    int first = 0;
    int size = 0;
};

// Fails unless the parallel pairs form classes of consecutive labels.
bool parallel_blocks(const ReplayResult& replayed,
                     std::vector<ParallelBlock>& blocks, std::string& reason);

// Prepends the crossings of line n, the line at infinity, with every old line.
// A parallel class meets it in one step, which reverses the class, so the
// result has to be replayed from closure_start(), not from 0..n.
Word add_line_at_infinity(const Word& word, int n,
                          const std::vector<ParallelBlock>& blocks = {});

// 0..n with every parallel class reversed.
std::vector<int> closure_start(int n, const std::vector<ParallelBlock>& blocks);

}  // namespace addline
