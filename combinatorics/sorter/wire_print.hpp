// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Roman Parpalak and Denis Utkin

#pragma once

#include "omatrix.hpp"

#include <cstddef>
#include <vector>

// Formatters that write to std::cout. Kept as a small ad-hoc visualisation
// layer for human-readable diagnostics; not on any hot path.

void print_OMatrix(OMatrix& o);

// Wiring diagram: one elementary crossing per row.
void print_wire(std::vector<std::size_t> gens);

// Condensed wiring diagram: commuting crossings stacked on the same row.
void print_wire_condensed(std::vector<std::size_t> gens);
