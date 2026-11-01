// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Roman Parpalak and Denis Utkin

#include "generator_parser.hpp"

#include <algorithm>
#include <charconv>
#include <cctype>
#include <cstdint>
#include <limits>
#include <sstream>
#include <system_error>
#include <utility>

namespace {

bool parse_size_t_token(const std::string& token, size_t& value) {
    if (token.empty())
        return false;

    for (char c : token) {
        if (!std::isdigit(static_cast<unsigned char>(c)))
            return false;
    }

    uint64_t parsed = 0;
    const char* begin = token.data();
    const char* end = token.data() + token.size();
    auto result = std::from_chars(begin, end, parsed);
    if (result.ec != std::errc() || result.ptr != end)
        return false;
    if (parsed > static_cast<uint64_t>(std::numeric_limits<size_t>::max()))
        return false;

    value = static_cast<size_t>(parsed);
    return true;
}

} // namespace

GeneratorParseResult parse_generators(const std::string& s) {
    GeneratorParseResult result;
    std::vector<size_t> numbers;
    std::string payload = s;
    size_t close_paren_pos = s.find(')');
    if (close_paren_pos != std::string::npos) {
        payload = s.substr(close_paren_pos + 1);
    }

    // Ordinary words keep the parser below, with its even-only shorthand and
    // diagnostics. A word with multiple points goes through the core parser.
    if (payload.find('*') != std::string::npos) {
        const addline::ParseResult grouped = addline::parse_word(payload);
        if (!grouped.ok) {
            result.reason = grouped.reason;
            return result;
        }

        result.word = grouped.word;
        for (const addline::Step& step : grouped.word) {
            if (step.k > 2) result.has_multiple_points = true;
            if (step.pos < 0 || step.k < 2) {
                result.reason = "invalid grouped generator";
                return result;
            }
            result.n = std::max(result.n,
                                static_cast<std::size_t>(step.pos + step.k));
            for (int d = 1; d < step.k; ++d) {
                for (int j = d - 1; j >= 0; --j) {
                    result.gens.push_back(static_cast<std::size_t>(step.pos + j));
                }
            }
        }
        result.ok = true;
        return result;
    }

    std::stringstream ss(payload);
    std::string token;
    while (ss >> token) {
        size_t value = 0;
        if (!parse_size_t_token(token, value)) {
            result.reason = "invalid generator token: " + token;
            return result;
        }
        numbers.push_back(value);
    }

    if (numbers.empty()) {
        result.reason = "empty or non-generator line";
        return result;
    }

    bool all_even = true;
    size_t max_generator = 0;
    for (size_t value : numbers) {
        if ((value & 1) != 0)
            all_even = false;
        if (value > max_generator)
            max_generator = value;
    }

    if (!all_even) {
        result.ok = true;
        result.gens = std::move(numbers);
        result.word.clear();
        for (std::size_t gen : result.gens) {
            result.word.push_back(addline::Step{static_cast<int>(gen), 2});
        }
        result.n = max_generator + 2;
        return result;
    }

    if (max_generator > std::numeric_limits<size_t>::max() - 3) {
        result.reason = "generator value is too large";
        return result;
    }

    const size_t n = max_generator + 3;
    std::vector<size_t> expanded;
    expanded.reserve((numbers.size() * 3) + (n / 2));

    for (size_t gen = 1; gen < n; gen += 2)
        expanded.push_back(gen);

    for (size_t gen : numbers) {
        expanded.push_back(gen);
        if (gen > 0)
            expanded.push_back(gen - 1);
        if (gen + 1 < n)
            expanded.push_back(gen + 1);
    }

    result.ok = true;
    result.expanded_even_generators = true;
    result.gens = std::move(expanded);
    result.word.clear();
    for (std::size_t gen : result.gens) {
        result.word.push_back(addline::Step{static_cast<int>(gen), 2});
    }
    result.n = n;
    return result;
}

std::vector<size_t> gens_str_to_vec(const std::string& s) {
    GeneratorParseResult parsed = parse_generators(s);
    if (!parsed.ok)
        return {};

    return parsed.gens;
}
