// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Roman Parpalak and Denis Utkin

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include "json_writer.hpp"
#include "omatrix.hpp"
#include "symmetry_analyzer.hpp"

#include <string>

namespace {

bool contains(const std::string& haystack, const std::string& needle) {
    return haystack.find(needle) != std::string::npos;
}

}  // namespace

TEST_CASE("json_escape_string handles special characters") {
    CHECK(json_escape_string("plain") == "plain");
    CHECK(json_escape_string("with \"quote\"") == "with \\\"quote\\\"");
    CHECK(json_escape_string("back\\slash") == "back\\\\slash");
    CHECK(json_escape_string("tab\there") == "tab\\there");
    CHECK(json_escape_string("new\nline") == "new\\nline");
    // Non-printable ASCII becomes \u00XX
    CHECK(json_escape_string(std::string(1, '\x01')) == "\\u0001");
}

TEST_CASE("write_analysis_json on synthetic minimal record") {
    SymmetryAnalysisResult r;
    r.pos = 7;
    r.n = 5;
    r.pid = "P5_abc";
    r.p_sym_ord = 1;
    r.p_sym_type = "C1";
    // No generators, identity-only group.
    r.p_sym_element_orders[1] = 1;
    r.cut_to_e_class = {0, 0, 0, 0, 0, 0};
    ELocalClass ec;
    ec.local_e_id = 0;
    ec.eid = "E5_zzz";
    ec.cut_count = 6;
    ec.representatives = 10;
    ec.stabilizer_type = "C1";
    ec.stabilizer_order = 1;
    ec.stabilizer_rotation_fixes = 1;
    ec.stabilizer_reflection_fixes = 0;
    r.e_classes.push_back(ec);
    r.checks = AnalysisChecks{true, true, true, true, true, true};

    JsonWriterOptions opts;
    auto json = write_analysis_json(r, opts);

    // No newlines.
    CHECK(json.find('\n') == std::string::npos);
    // Mandatory fields.
    CHECK(contains(json, "\"n\":5"));
    CHECK(contains(json, "\"pid\":\"P5_abc\""));
    CHECK(contains(json, "\"gp_ord\":1"));
    CHECK(contains(json, "\"gp_type\":\"C1\""));
    CHECK(contains(json, "\"gp_gens\":[]"));
    CHECK(contains(json, "\"gp_elt_orders\":{\"1\":1}"));
    CHECK(contains(json, "\"chart_to_e\":[0,0,0,0,0,0]"));
    CHECK(contains(json, "\"e_classes\":["));
    CHECK(contains(json, "\"eid\":\"E5_zzz\""));
    CHECK(contains(json, "\"m\":6"));
    CHECK(contains(json, "\"reps\":10"));
    CHECK(contains(json, "\"he_type\":\"C1\""));
    CHECK(contains(json, "\"he_ord\":1"));
    CHECK(contains(json, "\"he_gens\":[]"));
    CHECK(contains(json, "\"checks\":{"));
    CHECK(contains(json, "\"sum_m_ok\":true"));
    CHECK(contains(json, "\"orbit_stabilizer_ok\":true"));
    CHECK(contains(json, "\"representatives_ok\":true"));
    CHECK(contains(json, "\"generators_close\":true"));
    CHECK(contains(json, "\"e_generators_close\":true"));
    CHECK(contains(json, "\"chart_to_e_ok\":true"));

    // Dropped fields must not appear.
    CHECK_FALSE(contains(json, "\"pos\""));
    CHECK_FALSE(contains(json, "\"local_e_id\""));
    CHECK_FALSE(contains(json, "stabilizer_rotation_fixes"));
    CHECK_FALSE(contains(json, "stabilizer_reflection_fixes"));

    // No full perm fields by default.
    CHECK_FALSE(contains(json, "gp_elts"));
    CHECK_FALSE(contains(json, "he_elts"));

    // No gens field by default.
    CHECK_FALSE(contains(json, "\"gens\""));
}

TEST_CASE("write_analysis_json with include_full_perms adds full element lists") {
    SymmetryAnalysisResult r;
    r.pos = 1;
    r.n = 3;
    r.pid = "P3_x";
    r.p_sym_ord = 2;
    r.p_sym_type = "C2";
    r.p_sym_generators.push_back(LinePermutation({1, 0, 2, 3}));
    r.p_sym_elements.push_back(LinePermutation::identity(4));
    r.p_sym_elements.push_back(LinePermutation({1, 0, 2, 3}));
    r.p_sym_element_orders[1] = 1;
    r.p_sym_element_orders[2] = 1;
    r.cut_to_e_class = {0, 0, 0, 0};
    ELocalClass ec;
    ec.local_e_id = 0;
    ec.eid = "E3_x";
    ec.cut_count = 4;
    ec.stabilizer_type = "C2";
    ec.stabilizer_order = 2;
    ec.stabilizer_rotation_fixes = 2;
    ec.representatives = 3;
    ec.e_sym_generators.push_back(LinePermutation({1, 0, 2, 3}));
    ec.e_sym_elements.push_back(LinePermutation::identity(4));
    ec.e_sym_elements.push_back(LinePermutation({1, 0, 2, 3}));
    r.e_classes.push_back(ec);
    r.checks = AnalysisChecks{true, true, true, true, true, true};

    JsonWriterOptions opts;
    opts.include_full_perms = true;
    auto json = write_analysis_json(r, opts);

    CHECK(contains(json, "\"gp_elts\":[[0,1,2,3],[1,0,2,3]]"));
    CHECK(contains(json, "\"he_elts\":[[0,1,2,3],[1,0,2,3]]"));
    CHECK(contains(json, "\"gp_gens\":[[1,0,2,3]]"));
}

TEST_CASE("write_analysis_json with include_gens adds raw word") {
    SymmetryAnalysisResult r;
    r.pos = 1;
    r.n = 3;
    r.pid = "P3_x";
    r.p_sym_ord = 1;
    r.p_sym_type = "C1";
    r.p_sym_element_orders[1] = 1;
    r.cut_to_e_class = {0, 0, 0, 0};
    ELocalClass ec;
    ec.local_e_id = 0;
    ec.eid = "E3_x";
    ec.cut_count = 4;
    ec.stabilizer_type = "C1";
    ec.stabilizer_order = 1;
    ec.stabilizer_rotation_fixes = 1;
    ec.representatives = 6;
    r.e_classes.push_back(ec);
    r.checks = AnalysisChecks{true, true, true, true, true, true};

    JsonWriterOptions opts;
    opts.include_gens = true;
    opts.gens_string = "0 1 2 \"hi\"";
    auto json = write_analysis_json(r, opts);

    CHECK(contains(json, "\"gens\":\"0 1 2 \\\"hi\\\"\""));
}

TEST_CASE("write_analysis_json on real n=5 fixture") {
    std::vector<size_t> gens = {1, 3, 2, 1, 3, 0, 1, 2, 1, 3};
    auto o = get_OMatrix(gens);
    REQUIRE(o.has_value());

    AnalyzerOptions a_opts;
    a_opts.no_mirror = true;  // oriented; matches the p_sym_ord=30 assertion below
    auto res = analyze_symmetries(*o, /*pos=*/1, a_opts);
    REQUIRE(res.has_value());

    auto json = write_analysis_json(*res);

    CHECK(json.find('\n') == std::string::npos);
    CHECK(contains(json, "\"n\":5"));
    CHECK(contains(json, "\"gp_ord\":30"));
    CHECK(contains(json, "\"sum_m_ok\":true"));
}
