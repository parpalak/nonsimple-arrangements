// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Roman Parpalak and Denis Utkin

#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

extern const std::string B36_DIGITS;

std::string base36_encode(uint64_t value);
std::string eid_label(size_t n, uint64_t hash);
std::string pid_label(size_t n, uint64_t hash);
