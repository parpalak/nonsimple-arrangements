// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Roman Parpalak and Denis Utkin

#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "omatrix.hpp"

namespace addline {

// Classes are equal when their bytes are; the hash is only a printable label.
struct ExactKey {
    std::vector<std::uint8_t> bytes;
    std::uint64_t hash = 0;
};

struct ClassIdentifiers {
    bool ok = false;
    ExactKey d_key;
    ExactKey e_key;
    ExactKey p_key;
    std::string did;
    std::string eid;
    std::string pid;
    std::string reason;
};

// RELABEL maps the labels of the form to those of CHART, infinity at index n.
struct ChartVariant {
    OMatrix chart;
    std::vector<int> relabel;
};

// RELABEL maps the input labels to IMAGE labels and fixes infinity. ROTATION
// is in 0..2n-1; REFLECTED means the reflection was applied first.
struct EuclideanImage {
    OMatrix image;
    ExactKey d_key;
    std::vector<int> relabel;
    int rotation = 0;
    bool reflected = false;
};

// D and E are read off CHART, P off FORM.
ClassIdentifiers class_identifiers(const OMatrix& chart,
                                   const ProjectiveOMatrix& form);

// Every replayable chart with INFINITY at infinity. There is more than one
// only when INFINITY runs through a multiple point: they differ in the order
// of the resulting parallel class, which the projective form does not record.
bool chart_variants(const ProjectiveOMatrix& form, int infinity,
                    std::vector<ChartVariant>& out, std::string& reason);

// With parallel classes there are only m = n - sum (k_i - 1) directions and
// 2m rotations; the others are no chart and are skipped, so ROTATION is not a
// running count.
bool euclidean_images(const OMatrix& chart, bool include_reflections,
                      std::vector<EuclideanImage>& out,
                      std::string& reason);

bool exact_d_key(const OMatrix& o, ExactKey& out, std::string& reason);
bool exact_e_key(const OMatrix& o, ExactKey& out, std::string& reason);
bool exact_p_key(const ProjectiveOMatrix& p, ExactKey& out,
                 std::string& reason);

std::uint64_t fnv1a64(const std::vector<std::uint8_t>& bytes);
std::string base36(std::uint64_t value);

}  // namespace addline
