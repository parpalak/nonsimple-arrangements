// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Roman Parpalak and Denis Utkin

#include "configuration.hpp"

#include "id_labels.hpp"

#include <algorithm>
#include <numeric>

Configuration::Configuration(int line_number_in, const std::vector<std::size_t>& gens_in)
    : line_number(line_number_in), gens(gens_in) {
    o = get_OMatrix(gens);
}

Configuration::Configuration(int line_number_in, const GeneratorParseResult& parsed)
    : Configuration(line_number_in, parsed.gens) {
    grouped_word = parsed.word;
    multiple_points = parsed.has_multiple_points;
    const addline::OMatrixResult built =
        addline::omatrix_from_word(grouped_word, static_cast<int>(parsed.n));
    if (built.ok) {
        grouped_o = built.o;
        projective_o = addline::projective_closure(*grouped_o);
    }
}

namespace {

const addline::ClassIdentifiers* generalized_ids(const Configuration& conf) {
    if (!conf.grouped_o || !conf.projective_o) return nullptr;
    if (!conf.grouped_identifiers) {
        conf.grouped_identifiers =
            addline::class_identifiers(*conf.grouped_o, *conf.projective_o);
    }
    if (!conf.grouped_identifiers->ok) return nullptr;
    return &*conf.grouped_identifiers;
}

}  // namespace

const std::optional<OMatrix>& Configuration::get_min_o() const {
    if (!o)
        return o;
    if (min_o)
        return min_o;
    min_o = o->min_o();
    return min_o;
}

const std::optional<OMatrix>& Configuration::get_min_p_o() const {
    if (!o)
        return o;
    if (min_p_o)
        return min_p_o;
    min_p_o = o->min_projective_o();
    return min_p_o;
}

std::string Configuration::get_did() const {
    if (const auto* ids = generalized_ids(*this)) return ids->did;
    if (multiple_points) return {};
    std::string result;
    if (o.has_value()) {
        result += "D";
        result += std::to_string(o->n());
        result += "-";
        result += base36_encode(o->hash());
    }
    return result;
}

std::string Configuration::get_eid() const {
    if (const auto* ids = generalized_ids(*this)) return ids->eid;
    if (multiple_points) return {};
    std::string result;
    const std::optional<OMatrix>& m = get_min_o();
    if (m.has_value()) {
        result += "E";
        result += std::to_string(m->n());
        result += "-";
        result += base36_encode(m->hash());
    }
    return result;
}

std::string Configuration::get_pid() const {
    if (const auto* ids = generalized_ids(*this)) return ids->pid;
    if (multiple_points) return {};
    std::string result;
    const std::optional<OMatrix>& m = get_min_p_o();
    if (m.has_value()) {
        result += "P";
        result += std::to_string(m->n());
        result += "-";
        result += base36_encode(m->hash());
    }
    return result;
}

std::optional<std::vector<std::uint8_t>> Configuration::get_class_key(
    const std::string& mode) const {
    const auto* ids = generalized_ids(*this);
    if (!ids) return std::nullopt;
    if (mode == "D") return ids->d_key.bytes;
    if (mode == "E") return ids->e_key.bytes;
    if (mode == "P") return ids->p_key.bytes;
    return std::nullopt;
}

int Configuration::get_A() const {
    std::size_t n = 0;
    if (!gens.empty())
        n = *std::max_element(gens.begin(), gens.end()) + 2;

    int s = -1 + static_cast<int>(n & 1);
    for (std::size_t gen : gens) {
        s -= static_cast<int>((gen & 1) * 2 - 1);
    }
    return s;
}

std::vector<std::size_t> Configuration::get_perm() const {
    if (cached_perm.has_value())
        return *cached_perm;

    std::size_t n = 0;
    if (!gens.empty())
        n = *std::max_element(gens.begin(), gens.end()) + 2;

    std::vector<std::size_t> perm(n);
    std::iota(perm.begin(), perm.end(), 0);
    for (std::size_t gen : gens) {
        if (gen + 1 >= perm.size())
            continue;
        std::swap(perm[gen], perm[gen + 1]);
    }

    cached_perm = perm;
    return *cached_perm;
}

W0Validation Configuration::get_w0_validation() const {
    if (cached_w0_validation.has_value())
        return *cached_w0_validation;

    W0Validation result;
    if (gens.empty()) {
        result.status = "empty";
        result.reason = "empty generator list";
        cached_w0_validation = result;
        return *cached_w0_validation;
    }

    const std::size_t n = *std::max_element(gens.begin(), gens.end()) + 2;
    for (std::size_t gen : gens) {
        if (gen >= n - 1) {
            result.status = "bad_generator";
            result.reason = "generator " + std::to_string(gen) +
                            " is out of range 0.." + std::to_string(n - 2);
            cached_w0_validation = result;
            return *cached_w0_validation;
        }
    }

    const std::size_t expected_length = n * (n - 1) / 2;
    if (gens.size() != expected_length) {
        result.status = "bad_length";
        result.reason = "length " + std::to_string(gens.size()) +
                        ", expected " + std::to_string(expected_length);
        cached_w0_validation = result;
        return *cached_w0_validation;
    }

    std::vector<std::size_t> perm = get_perm();
    for (std::size_t i = 0; i < n; ++i) {
        if (perm[i] != n - 1 - i) {
            result.status = "bad_target";
            result.reason = "result permutation is not w0";
            cached_w0_validation = result;
            return *cached_w0_validation;
        }
    }

    result.ok = true;
    result.status = "ok";
    result.reason = "reduced word for w0";
    cached_w0_validation = result;
    return *cached_w0_validation;
}
