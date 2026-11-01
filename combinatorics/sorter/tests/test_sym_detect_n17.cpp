// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Roman Parpalak and Denis Utkin

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include "exact_key.hpp"
#include "generator_parser.hpp"
#include "json_writer.hpp"
#include "omatrix.hpp"
#include "symmetry_analyzer.hpp"

#include <fstream>
#include <optional>
#include <string>
#include <vector>

namespace {

#ifndef PSEUDOLINE_DATA_DIR
#error "PSEUDOLINE_DATA_DIR must be set by the build system"
#endif
const char* kInputPath = PSEUDOLINE_DATA_DIR "/exhaustive/17.uniq-p.txt";

std::vector<SymmetryAnalysisResult> run_file(const std::string& path,
                                             const AnalyzerOptions& opts) {
    std::ifstream in(path);
    REQUIRE_MESSAGE(in.good(), "could not open input file: ", path);

    std::vector<SymmetryAnalysisResult> out;
    std::string line;
    std::optional<std::size_t> expected_n;
    std::size_t pos = 0;
    while (std::getline(in, line)) {
        ++pos;
        GeneratorParseResult parsed = parse_generators(line);
        if (!parsed.ok)
            continue;
        std::string reason;
        auto o = get_exact_cache_OMatrix(parsed.gens, reason);
        if (!o.has_value())
            continue;
        if (!validate_exact_cache_input_n(o->n(), expected_n, reason))
            continue;
        auto r = analyze_symmetries(*o, pos, opts);
        REQUIRE(r.has_value());
        out.push_back(std::move(*r));
    }
    return out;
}

}  // namespace

TEST_CASE("sym_detect on n=17: 3 records, basic invariants in mirror mode") {
    AnalyzerOptions opts;
    opts.no_mirror = false;
    auto results = run_file(kInputPath, opts);
    REQUIRE(results.size() == 3);

    for (const auto& r : results) {
        CHECK(r.n == 17);
        // In mirror mode all six checks should pass (full D_4n action).
        CHECK(r.checks.sum_m_ok);
        CHECK(r.checks.orbit_stabilizer_ok);
        CHECK(r.checks.representatives_ok);
        CHECK(r.checks.chart_to_e_ok);
        CHECK(r.checks.generators_close);
        CHECK(r.checks.e_generators_close);
        // The symmetry enumeration never deduplicates (rho^n fixes no
        // O-matrix), so this invariant holds in both modes.
        CHECK(r.checks.kernel_collapse_free);
    }

    // JSON serialization sanity.
    for (const auto& r : results) {
        std::string js = write_analysis_json(r);
        CHECK(js.find("\"n\":17") != std::string::npos);
    }
}

TEST_CASE("sym_detect on n=17 (oriented): 3 records, structural invariants") {
    // Oriented mode is known to fail strict consistency checks on chiral
    // configurations: enumerated p_sym_perms need not close under composition,
    // and the post-cut stabilizer measured under T = 2n misses reflections.
    // We assert only the always-valid combinatorial invariants here.
    AnalyzerOptions opts;
    opts.no_mirror = true;
    auto results = run_file(kInputPath, opts);
    REQUIRE(results.size() == 3);

    for (const auto& r : results) {
        CHECK(r.n == 17);
        CHECK(r.checks.sum_m_ok);
        CHECK(r.checks.chart_to_e_ok);
        CHECK(r.checks.representatives_ok);  // T / stab_order * stab_order = T tautologically
        CHECK(r.checks.kernel_collapse_free);  // holds regardless of mirror mode
    }
}
