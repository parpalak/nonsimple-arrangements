// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Roman Parpalak and Denis Utkin

#include "line_permutation_group.hpp"

#include <algorithm>
#include <set>
#include <stdexcept>
#include <string>

namespace {

std::string classify(size_t order,
                     const std::map<size_t, size_t>& element_orders,
                     bool is_abelian) {
    if (order == 1)
        return "C1";

    auto get = [&](size_t k) -> size_t {
        auto it = element_orders.find(k);
        return it == element_orders.end() ? 0 : it->second;
    };

    if (is_abelian) {
        size_t max_order = 0;
        for (auto& [ord, _] : element_orders)
            if (ord > max_order)
                max_order = ord;
        if (max_order == order)
            return "C" + std::to_string(order);
        if (order == 4 && get(2) == 3)
            return "D2";  // Klein four-group; D_2 in the |D_k|=2k convention
        return "abelian order " + std::to_string(order);
    }

    if (order == 6)
        return "D3/S3";
    if (order == 8 && get(4) > 0)
        return "D4 or Q8";
    if (order == 12 && get(2) == 3 && get(3) == 8)
        return "A4";
    if (order == 12 && get(2) == 7 && get(3) == 2 && get(6) == 2)
        return "D6";
    if (order == 24 && get(2) == 9 && get(3) == 8 && get(4) == 6)
        return "S4";
    if (order == 60 && get(2) == 15 && get(3) == 20 && get(5) == 24)
        return "A5";  // icosahedral rotation group, unique nonabelian of order 60

    return "nonabelian order " + std::to_string(order);
}

bool check_abelian(const std::vector<LinePermutation>& elements) {
    for (const auto& l : elements) {
        for (const auto& r : elements) {
            if (l.compose(r) != r.compose(l))
                return false;
        }
    }
    return true;
}

std::vector<LinePermutation> find_generators(const std::vector<LinePermutation>& elements) {
    const size_t target = elements.size();
    if (target <= 1)
        return {};

    // 1. Single generator with element order == target.
    for (const auto& e : elements) {
        if (e.order() == target)
            return {e};
    }

    // 2. Pair generation in lex order. Elements are already sorted (kept that way
    // by from_elements via std::set ordering).
    for (const auto& first : elements) {
        if (first.order() == 1)
            continue;
        for (const auto& second : elements) {
            if (second.order() == 1)
                continue;
            if (first == second)
                continue;
            std::vector<LinePermutation> pair = {first, second};
            if (LinePermutationGroup::closure_size(pair) == target)
                return pair;
        }
    }

    // 3. Greedy.
    std::vector<LinePermutation> gens;
    for (const auto& e : elements) {
        if (e.order() == 1)
            continue;
        gens.push_back(e);
        if (LinePermutationGroup::closure_size(gens) == target)
            return gens;
    }

    return gens;
}

}  // namespace

LinePermutationGroup LinePermutationGroup::from_elements(
    const std::vector<LinePermutation>& input) {
    if (input.empty())
        throw std::invalid_argument("LinePermutationGroup requires at least one element");

    // Deduplicate while preserving sorted order for stable generator selection.
    std::set<LinePermutation> dedup;
    for (const auto& e : input)
        dedup.insert(e);
    std::vector<LinePermutation> elements(dedup.begin(), dedup.end());

    const size_t size = elements.front().size();
    for (const auto& e : elements) {
        if (e.size() != size)
            throw std::invalid_argument("All group permutations must have the same size");
    }

    std::map<size_t, size_t> orders;
    for (const auto& e : elements) {
        orders[e.order()]++;
    }

    bool abelian = check_abelian(elements);
    auto gens = find_generators(elements);

    LinePermutationGroup g;
    g.elements_ = std::move(elements);
    g.element_orders_ = std::move(orders);
    g.generators_ = std::move(gens);
    g.is_abelian_ = abelian;
    g.type_ = classify(g.elements_.size(), g.element_orders_, g.is_abelian_);
    return g;
}

size_t LinePermutationGroup::closure_size(const std::vector<LinePermutation>& generators) {
    if (generators.empty())
        return 1;

    const size_t size = generators.front().size();
    LinePermutation identity = LinePermutation::identity(size);
    std::vector<LinePermutation> steps;
    steps.reserve(generators.size() * 2);
    for (const auto& g : generators) {
        steps.push_back(g);
        steps.push_back(g.inverse());
    }

    std::set<LinePermutation> seen;
    seen.insert(identity);
    std::vector<LinePermutation> frontier;
    frontier.push_back(identity);

    while (!frontier.empty()) {
        LinePermutation current = frontier.back();
        frontier.pop_back();
        for (const auto& step : steps) {
            LinePermutation next = current.compose(step);
            if (seen.insert(next).second)
                frontier.push_back(next);
        }
    }

    return seen.size();
}

std::vector<LinePermutation> LinePermutationGroup::closure(
    const std::vector<LinePermutation>& generators, size_t size) {
    LinePermutation identity = LinePermutation::identity(size);
    std::set<LinePermutation> seen;
    seen.insert(identity);

    if (generators.empty())
        return {identity};

    std::vector<LinePermutation> steps;
    steps.reserve(generators.size() * 2);
    for (const auto& g : generators) {
        if (g.size() != size)
            throw std::invalid_argument("closure: generator size mismatch");
        steps.push_back(g);
        steps.push_back(g.inverse());
    }

    std::vector<LinePermutation> frontier{identity};
    while (!frontier.empty()) {
        LinePermutation current = frontier.back();
        frontier.pop_back();
        for (const auto& step : steps) {
            LinePermutation next = current.compose(step);
            if (seen.insert(next).second)
                frontier.push_back(next);
        }
    }

    return std::vector<LinePermutation>(seen.begin(), seen.end());
}
