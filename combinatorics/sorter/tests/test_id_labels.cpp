// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Roman Parpalak and Denis Utkin

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include "id_labels.hpp"

TEST_CASE("base36_encode of zero is \"0\"") {
    CHECK(base36_encode(0) == "0");
}

TEST_CASE("base36_encode of small known values") {
    CHECK(base36_encode(1) == "1");
    CHECK(base36_encode(9) == "9");
    CHECK(base36_encode(10) == "a");
    CHECK(base36_encode(35) == "z");
    CHECK(base36_encode(36) == "10");
    CHECK(base36_encode(37) == "11");
    CHECK(base36_encode(71) == "1z");
    CHECK(base36_encode(72) == "20");
    CHECK(base36_encode(1295) == "zz");
    CHECK(base36_encode(1296) == "100");
}

TEST_CASE("eid_label format") {
    CHECK(eid_label(5, 0) == "E5-0");
    CHECK(eid_label(9, 36) == "E9-10");
}

TEST_CASE("pid_label format") {
    CHECK(pid_label(5, 0) == "P5-0");
    CHECK(pid_label(9, 36) == "P9-10");
}
