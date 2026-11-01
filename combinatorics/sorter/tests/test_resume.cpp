// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Roman Parpalak and Denis Utkin

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include "resume.hpp"
#include "exact_key.hpp"
#include "state_io.hpp"

#include <cstdio>
#include <fstream>
#include <optional>
#include <string>
#include <vector>

namespace {

// Known-good reduced words reused from test_exact_key. Their literal form
// mixes odd and even tokens, so parse_generators keeps them verbatim (the
// even-expansion path only triggers on all-even input).
std::vector<size_t> n5_gens() {
    return {1, 3, 2, 1, 3, 0, 1, 2, 1, 3};
}

std::vector<size_t> n9_gens() {
    return {1, 3, 5, 7, 6, 5, 4, 3, 2, 1, 0, 1, 3, 5, 4, 3, 2,
            1, 3, 5, 7, 6, 5, 4, 3, 2, 1, 3, 5, 7, 6, 5, 4, 3, 5, 7};
}

std::string gens_to_line(const std::vector<size_t>& gens) {
    std::string s;
    for (size_t i = 0; i < gens.size(); ++i) {
        if (i)
            s += ' ';
        s += std::to_string(gens[i]);
    }
    return s;
}

void write_lines(const std::string& path, const std::vector<std::string>& lines) {
    std::ofstream f(path, std::ios::trunc);
    for (const auto& l : lines)
        f << l << "\n";
}

// Removes the file when it goes out of scope, so a failing CHECK does not
// leave the temp file behind.
struct TmpFile {
    std::string path;
    explicit TmpFile(std::string p) : path(std::move(p)) {}
    ~TmpFile() { std::remove(path.c_str()); }
};

} // namespace

// ----------------------------------------------------------------------------
// parse_resume_line: pure extraction of pid/eid prefix tokens + generators.
// ----------------------------------------------------------------------------

TEST_CASE("parse_resume_line extracts pid, eid and generators from a full output line") {
    auto info = parse_resume_line("P5-abc E5-def ) 1 3 2 1 3 0 1 2 1 3");

    CHECK(info.parse_ok);
    REQUIRE(info.expected_pid.has_value());
    CHECK(*info.expected_pid == "P5-abc");
    REQUIRE(info.expected_eid.has_value());
    CHECK(*info.expected_eid == "E5-def");
    CHECK(info.gens == std::vector<size_t>{1, 3, 2, 1, 3, 0, 1, 2, 1, 3});
}

TEST_CASE("parse_resume_line: a bare generator line carries no pid/eid") {
    auto info = parse_resume_line("1 3 2 1 3 0 1 2 1 3");

    CHECK(info.parse_ok);
    CHECK_FALSE(info.expected_pid.has_value());
    CHECK_FALSE(info.expected_eid.has_value());
    CHECK(info.gens == std::vector<size_t>{1, 3, 2, 1, 3, 0, 1, 2, 1, 3});
}

TEST_CASE("parse_resume_line accepts the legacy underscore id separator") {
    auto info = parse_resume_line("P5_abc ) 1 0 1");

    REQUIRE(info.expected_pid.has_value());
    CHECK(*info.expected_pid == "P5_abc");
}

TEST_CASE("parse_resume_line: a short P token is not mistaken for a pid") {
    auto info = parse_resume_line("Px ) 1 0 1");

    CHECK_FALSE(info.expected_pid.has_value());
}

TEST_CASE("parse_resume_line reports a parse error on a non-generator line") {
    auto info = parse_resume_line("totally bogus input");

    CHECK_FALSE(info.parse_ok);
    CHECK_FALSE(info.parse_error.empty());
}

// ----------------------------------------------------------------------------
// load_exact_cache_resume_output: reconstruct ExactCacheState from a previous
// text output file.
// ----------------------------------------------------------------------------

TEST_CASE("load_exact_cache_resume_output reconstructs the arenas a direct insertion would build") {
    const auto gens = n5_gens();
    std::string reason;
    auto o = get_exact_cache_OMatrix(gens, reason);
    REQUIRE(o.has_value());
    const size_t n = o->n();
    auto orbit = get_projective_orbit_keys(*o, n);
    REQUIRE(orbit.has_value());

    // Reference arenas: mirror exactly what the loader is expected to do
    // (insert every E-key into covered_e, the single P-key into seen_p).
    FixedLenExactKeyArena ref_e;
    ref_e.init(n);
    for (const auto& k : orbit->e_keys)
        ref_e.insert_if_absent(k);
    FixedLenExactKeyArena ref_p;
    ref_p.init(n);
    ref_p.insert_if_absent(orbit->p_key);

    TmpFile tmp("test_resume_roundtrip.txt");
    write_lines(tmp.path, {gens_to_line(gens)});

    ExactCacheState state;
    int rc = load_exact_cache_resume_output(tmp.path, 0, 0.0, state);

    CHECK(rc == 0);
    REQUIRE(state.expected_n.has_value());
    CHECK(*state.expected_n == n);
    CHECK(state.seen_p.size() == 1);
    CHECK(state.covered_e.size() == ref_e.size());
    CHECK(state.seen_p.size() == ref_p.size());
    for (const auto& k : orbit->e_keys)
        CHECK(state.covered_e.contains(k));
    CHECK(state.seen_p.contains(orbit->p_key));
}

TEST_CASE("load_exact_cache_resume_output deduplicates a repeated configuration") {
    TmpFile tmp("test_resume_dedup.txt");
    write_lines(tmp.path, {gens_to_line(n5_gens()), gens_to_line(n5_gens())});

    ExactCacheState state;
    int rc = load_exact_cache_resume_output(tmp.path, 0, 0.0, state);

    CHECK(rc == 0);
    CHECK(state.seen_p.size() == 1);
}

TEST_CASE("load_exact_cache_resume_output skips a record whose n differs from the established stream n") {
    TmpFile tmp("test_resume_mixedn.txt");
    // First line fixes expected_n = 5; the n = 9 line must be rejected.
    write_lines(tmp.path, {gens_to_line(n5_gens()), gens_to_line(n9_gens())});

    ExactCacheState state;
    int rc = load_exact_cache_resume_output(tmp.path, 0, 0.0, state);

    CHECK(rc == 0);
    REQUIRE(state.expected_n.has_value());
    CHECK(*state.expected_n == 5);
    CHECK(state.seen_p.size() == 1);
}

TEST_CASE("load_exact_cache_resume_output skips an unparseable line and loads the rest") {
    TmpFile tmp("test_resume_badline.txt");
    write_lines(tmp.path, {"totally bogus input", gens_to_line(n5_gens())});

    ExactCacheState state;
    int rc = load_exact_cache_resume_output(tmp.path, 0, 0.0, state);

    CHECK(rc == 0);
    CHECK(state.seen_p.size() == 1);
}

TEST_CASE("load_exact_cache_resume_output returns -1 for a missing file") {
    ExactCacheState state;
    int rc = load_exact_cache_resume_output("test_resume_no_such_file_xyz.txt", 0, 0.0, state);

    CHECK(rc == -1);
}
