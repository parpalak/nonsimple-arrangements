// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Roman Parpalak and Denis Utkin

#pragma once

#include "traced_transform.hpp"

#include <cstddef>
#include <map>
#include <string>
#include <vector>

class LinePermutationGroup {
public:
    // Build a group from explicitly enumerated elements (deduplicated).
    static LinePermutationGroup from_elements(const std::vector<LinePermutation>& elements);

    // Compute the closure size of a generator set via BFS.
    // Empty generators -> identity-only group of order 1.
    static size_t closure_size(const std::vector<LinePermutation>& generators);

    // Compute the closure of generators as a sorted set of elements.
    static std::vector<LinePermutation> closure(const std::vector<LinePermutation>& generators,
                                                size_t size);

    size_t order() const { return elements_.size(); }
    const std::vector<LinePermutation>& elements() const { return elements_; }
    const std::map<size_t, size_t>& element_orders() const { return element_orders_; }
    const std::vector<LinePermutation>& generators() const { return generators_; }
    bool is_abelian() const { return is_abelian_; }
    const std::string& type() const { return type_; }

private:
    LinePermutationGroup() = default;

    std::vector<LinePermutation> elements_;
    std::map<size_t, size_t> element_orders_;
    std::vector<LinePermutation> generators_;
    bool is_abelian_ = false;
    std::string type_;
};
