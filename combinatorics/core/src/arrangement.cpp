// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Roman Parpalak and Denis Utkin

#include "arrangement.hpp"

#include <algorithm>
#include <numeric>

namespace addline {

ReplayResult replay(const Word& word, int n) {
    std::vector<int> initial(n > 0 ? n : 0);
    std::iota(initial.begin(), initial.end(), 0);
    return replay(word, n, initial);
}

ReplayResult replay(const Word& word, int n, const std::vector<int>& initial) {
    ReplayResult result;
    if (n < 2) {
        result.reason = "n below 2";
        return result;
    }
    if (static_cast<int>(initial.size()) != n) {
        result.reason = "initial order does not match n";
        return result;
    }
    {
        std::vector<char> seen(static_cast<std::size_t>(n), 0);
        for (int wire : initial) {
            if (wire < 0 || wire >= n || seen[wire]) {
                result.reason = "initial order is not a permutation";
                return result;
            }
            seen[wire] = 1;
        }
    }
    std::vector<int> slots = initial;

    Events events;
    events.n = n;
    events.on.assign(n, {});
    events.at.reserve(word.size());

    // crossed[a * n + b] guards the "every pair meets exactly once" rule.
    std::vector<char> crossed(static_cast<std::size_t>(n) * n, 0);

    for (const Step& step : word) {
        if (step.pos < 0 || step.pos + step.k > n) {
            result.reason = "generator out of range";
            return result;
        }
        std::vector<int> wires(slots.begin() + step.pos,
                              slots.begin() + step.pos + step.k);
        const int event = static_cast<int>(events.at.size());
        for (std::size_t i = 0; i < wires.size(); ++i) {
            for (std::size_t j = i + 1; j < wires.size(); ++j) {
                const std::size_t key =
                    static_cast<std::size_t>(wires[i]) * n + wires[j];
                const std::size_t mirror =
                    static_cast<std::size_t>(wires[j]) * n + wires[i];
                if (crossed[key]) {
                    result.reason = "lines cross twice";
                    return result;
                }
                crossed[key] = crossed[mirror] = 1;
            }
        }
        for (int wire : wires) events.on[wire].push_back(event);
        events.at.push_back(wires);
        std::reverse(slots.begin() + step.pos, slots.begin() + step.pos + step.k);
    }

    result.parallel.assign(static_cast<std::size_t>(n), {});
    for (int i = 0; i < n; ++i) {
        for (int j = i + 1; j < n; ++j) {
            if (!crossed[static_cast<std::size_t>(i) * n + j]) {
                result.parallel[i].push_back(j);
                result.parallel[j].push_back(i);
            }
        }
    }
    for (std::vector<int>& list : result.parallel) {
        std::sort(list.begin(), list.end());
    }

    result.ok = true;
    result.events = std::move(events);
    return result;
}

bool parallel_blocks(const ReplayResult& replayed,
                     std::vector<ParallelBlock>& blocks, std::string& reason) {
    blocks.clear();
    const int n = static_cast<int>(replayed.parallel.size());
    for (int line = 0; line < n;) {
        const std::vector<int>& mates = replayed.parallel[line];
        if (mates.empty()) {
            ++line;
            continue;
        }
        const int size = static_cast<int>(mates.size()) + 1;
        if (mates.front() != line + 1 || mates.back() != line + size - 1) {
            reason = "parallel lines of one point are not consecutive at line " +
                     std::to_string(line);
            return false;
        }
        // Every member has to see the same class, itself excluded.
        for (int i = 1; i < size; ++i) {
            const std::vector<int>& other = replayed.parallel[line + i];
            if (static_cast<int>(other.size()) != size - 1) {
                reason = "parallel classes disagree at line " +
                         std::to_string(line + i);
                return false;
            }
            for (int j = 0; j < size; ++j) {
                if (j == i) continue;
                if (std::find(other.begin(), other.end(), line + j) == other.end()) {
                    reason = "parallel classes disagree at line " +
                             std::to_string(line + i);
                    return false;
                }
            }
        }
        blocks.push_back(ParallelBlock{line, size});
        line += size;
    }
    return true;
}

std::vector<int> closure_start(int n, const std::vector<ParallelBlock>& blocks) {
    std::vector<int> order(static_cast<std::size_t>(n) + 1);
    std::iota(order.begin(), order.end(), 0);
    for (const ParallelBlock& block : blocks) {
        std::reverse(order.begin() + block.first,
                     order.begin() + block.first + block.size);
    }
    return order;
}

Word add_line_at_infinity(const Word& word, int n,
                          const std::vector<ParallelBlock>& blocks) {
    std::vector<int> block_of(static_cast<std::size_t>(n), -1);
    for (std::size_t b = 0; b < blocks.size(); ++b) {
        for (int i = 0; i < blocks[b].size; ++i) {
            block_of[static_cast<std::size_t>(blocks[b].first) + i] =
                static_cast<int>(b);
        }
    }

    Word out;
    out.reserve(word.size() + static_cast<std::size_t>(n));
    // A parallel class, reversed by closure_start(), is crossed in one step,
    // which reverses it back.
    for (int pos = n - 1; pos >= 0;) {
        const int b = block_of[pos];
        if (b < 0) {
            out.push_back(Step{pos, 2});
            --pos;
        } else {
            out.push_back(Step{blocks[b].first, blocks[b].size + 1});
            pos = blocks[b].first - 1;
        }
    }
    for (const Step& step : word) out.push_back(Step{step.pos + 1, step.k});
    return out;
}

}  // namespace addline
