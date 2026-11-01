// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Roman Parpalak and Denis Utkin

#pragma once

#include "exact_key.hpp"
#include "line_permutation_group.hpp"
#include "omatrix.hpp"
#include "traced_transform.hpp"
#include "identifiers.hpp"

#include <cstddef>
#include <map>
#include <optional>
#include <string>
#include <vector>

struct ELocalClass {
    size_t local_e_id = 0;
    std::string eid;                       // eid_label(n, key.hash)
    size_t cut_count = 0;
    size_t representatives = 0;
    size_t representatives_orbit = 0;       // orbit the count came from
    bool representatives_defined = true;
    std::string stabilizer_type;
    size_t stabilizer_order = 0;            // |H_E| in D_{2n}
    size_t stabilizer_image_order = 0;      // order of its line-permutation image
    size_t stabilizer_kernel_order = 1;     // |H_E| / |image(H_E)|
    size_t stabilizer_rotation_fixes = 0;
    size_t stabilizer_reflection_fixes = 0;
    std::vector<LinePermutation> e_sym_generators;       // post-cut coordinates
    std::vector<LinePermutation> e_sym_elements;         // full group, post-cut coords
    std::map<size_t, size_t> e_sym_element_orders;
};

struct AnalysisChecks {
    bool sum_m_ok = false;
    bool orbit_stabilizer_ok = false;
    bool representatives_ok = false;
    bool generators_close = false;
    bool e_generators_close = false;
    bool chart_to_e_ok = false;
    // In the simple setting no two matching operations induce the same line
    // permutation (arXiv:2607.29236, Lemma C.2).  Generalized point blocks may be
    // fixed by rho^n, in which case this diagnostic is legitimately false
    // and stabilizer_kernel_order records the quotient.
    bool kernel_collapse_free = false;
    bool representatives_applicable = true;
};

struct SymmetryAnalysisResult {
    size_t pos = 0;
    size_t n = 0;
    std::string pid;
    size_t p_sym_ord = 0;
    std::string p_sym_type;
    std::vector<LinePermutation> p_sym_generators;
    std::vector<LinePermutation> p_sym_elements;
    std::map<size_t, size_t> p_sym_element_orders;
    std::vector<size_t> cut_to_e_class;     // length n+1, indexed [0..n]
    std::vector<ELocalClass> e_classes;
    AnalysisChecks checks;
};

struct AnalyzerOptions {
    bool no_mirror = false;                 // false: T = 4n (default); true: T = 2n (oriented)
};

// Run the full symmetry analysis on a P-class representative O-matrix.
// Returns std::nullopt only if the input matrix violates basic invariants
// (e.g. projective rotation produces no valid configurations).
std::optional<SymmetryAnalysisResult> analyze_symmetries(
    const OMatrix& matrix,
    size_t pos,
    const AnalyzerOptions& options);

// Generalized counterpart retaining finite multiple points and parallel
// classes produced when a projective multiple point lies at infinity.
std::optional<SymmetryAnalysisResult> analyze_symmetries(
    const addline::OMatrix& matrix,
    const addline::ProjectiveOMatrix& form,
    size_t pos,
    const AnalyzerOptions& options);
