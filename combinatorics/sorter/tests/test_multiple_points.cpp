// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Roman Parpalak and Denis Utkin

#include "doctest.h"

#include "configuration.hpp"
#include "generator_parser.hpp"

#include <string>

namespace {

Configuration configuration_of(const std::string& word) {
    const GeneratorParseResult parsed = parse_generators(word);
    REQUIRE_MESSAGE(parsed.ok, parsed.reason);
    return Configuration(1, parsed);
}

}  // namespace

TEST_CASE("generalized identifiers preserve the legacy simple-word labels") {
    Configuration conf = configuration_of("0 1 0");

    CHECK(conf.get_did() == "D3-1u0c5a5eszdz");
    CHECK(conf.get_eid() == "E3-1u0c5a5eszdz");
    CHECK(conf.get_pid() == "P3-1u0c5a5eszdz");
    CHECK(conf.get_class_key("P").has_value());
}

TEST_CASE("a triple point survives parsing and all three identifiers") {
    Configuration conf = configuration_of("0*");

    CHECK(conf.has_multiple_points());
    const addline::Word triple{{0, 3}};
    CHECK(conf.grouped_word == triple);
    CHECK(conf.get_did().rfind("D3-", 0) == 0);
    CHECK(conf.get_eid().rfind("E3-", 0) == 0);
    CHECK(conf.get_pid().rfind("P3-", 0) == 0);
    CHECK(conf.get_class_key("D").has_value());
    CHECK(conf.get_class_key("E").has_value());
    CHECK(conf.get_class_key("P").has_value());
}

TEST_CASE("two adjacent triple points differ from one fivefold point") {
    Configuration two = configuration_of("0* 2* 1 0 2 1");
    Configuration five = configuration_of("0***");

    const auto two_key = two.get_class_key("D");
    const auto five_key = five.get_class_key("D");
    REQUIRE(two_key.has_value());
    REQUIRE(five_key.has_value());
    CHECK(*two_key != *five_key);
    CHECK(two.get_did() != five.get_did());
}

TEST_CASE("identifiers support a multiple point at infinity") {
    // The final order is 3,0,1,2: lines 0,1,2 are one parallel class.
    // The legacy sorter O-matrix only supports parallel pairs, while the
    // generalized matrix records this class as a fourfold projective point.
    Configuration conf = configuration_of("2 1 0");

    CHECK_FALSE(conf.o.has_value());
    REQUIRE(conf.grouped_o.has_value());
    CHECK(conf.get_did().rfind("D4-", 0) == 0);
    CHECK(conf.get_eid().rfind("E4-", 0) == 0);
    CHECK(conf.get_pid().rfind("P4-", 0) == 0);
    CHECK(conf.get_class_key("P").has_value());
}
