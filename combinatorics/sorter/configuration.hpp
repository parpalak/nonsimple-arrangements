// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Roman Parpalak and Denis Utkin

#pragma once

#include "omatrix.hpp"
#include "generator_parser.hpp"
#include "identifiers.hpp"

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

struct W0Validation {
    bool ok = false;
    std::string status;
    std::string reason;
};

// One input record: source line, generator list, optionally a parsed
// O-matrix and lazily-cached canonical forms / identifiers. Caches are
// mutable so they can be filled inside const accessors.
struct Configuration {
    int line_number;
    std::vector<std::size_t> gens;
    std::optional<OMatrix> o;
    mutable std::optional<OMatrix> min_o;
    mutable std::optional<OMatrix> min_p_o;
    mutable std::optional<std::vector<std::size_t>> cached_perm;
    mutable std::optional<W0Validation> cached_w0_validation;
    addline::Word grouped_word;
    std::optional<addline::OMatrix> grouped_o;
    std::optional<addline::ProjectiveOMatrix> projective_o;
    mutable std::optional<addline::ClassIdentifiers> grouped_identifiers;
    bool multiple_points = false;

    Configuration(int line_number, const std::vector<std::size_t>& gens);
    Configuration(int line_number, const GeneratorParseResult& parsed);

    const std::optional<OMatrix>& get_min_o() const;
    const std::optional<OMatrix>& get_min_p_o() const;

    std::string get_did() const;  // D<n>-<hash>
    std::string get_eid() const;  // E<n>-<hash>
    std::string get_pid() const;  // P<n>-<hash>
    std::optional<std::vector<std::uint8_t>> get_class_key(
        const std::string& mode) const;
    bool has_multiple_points() const { return multiple_points; }

    int get_A() const;
    std::vector<std::size_t> get_perm() const;
    W0Validation get_w0_validation() const;
};
