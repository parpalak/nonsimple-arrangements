// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Roman Parpalak and Denis Utkin

#include "resume.hpp"

#include "exact_key.hpp"
#include "generator_parser.hpp"
#include "id_labels.hpp"
#include "progress.hpp"

#include <fstream>
#include <iostream>
#include <sstream>
#include <utility>

ResumeLineInfo parse_resume_line(const std::string& line) {
    ResumeLineInfo info;
    std::size_t close_paren_pos = line.find(')');
    if (close_paren_pos != std::string::npos) {
        std::stringstream prefix(line.substr(0, close_paren_pos));
        std::string token;
        // The separator is '-' (canonical); '_' is accepted for resume from
        // output files produced before the format change.
        auto is_id_token = [](const std::string& t) {
            return t.size() > 2 && (t.find('-') != std::string::npos ||
                                    t.find('_') != std::string::npos);
        };
        while (prefix >> token) {
            if (token[0] == 'P' && is_id_token(token))
                info.expected_pid = token;
            else if (token[0] == 'E' && is_id_token(token))
                info.expected_eid = token;
        }
    }

    GeneratorParseResult parsed = parse_generators(line);
    info.parse_ok = parsed.ok;
    info.parse_error = parsed.reason;
    info.gens = std::move(parsed.gens);
    return info;
}

int load_exact_cache_resume_output(const std::string& path,
                                   std::size_t skip_lines_for_logging,
                                   double progress_interval_sec,
                                   ExactCacheState& state) {
    std::ifstream file(path);
    if (!file) {
        std::cerr << "Could not open resume output file " << path << std::endl;
        return -1;
    }

    std::cerr << "[resume] start output=" << path
              << " skip_lines=" << skip_lines_for_logging
              << std::endl;

    std::string line;
    std::size_t output_lines = 0;
    std::size_t resume_configs = 0;
    std::size_t resume_skipped_invalid = 0;
    std::size_t full_orbits = 0;
    progress::ProgressTimer progress_timer;

    while (std::getline(file, line)) {
        ++output_lines;

        ResumeLineInfo parsed = parse_resume_line(line);
        if (!parsed.parse_ok) {
            ++resume_skipped_invalid;
            std::cerr << "[resume] warning skipped_invalid_output line=" << output_lines
                      << " reason=\"" << parsed.parse_error << "\""
                      << std::endl;
            continue;
        }

        std::string reason;
        auto o = get_exact_cache_OMatrix(parsed.gens, reason);
        if (!o.has_value()) {
            ++resume_skipped_invalid;
            std::cerr << "[resume] warning skipped_invalid_output line=" << output_lines
                      << " reason=\"" << reason << "\""
                      << std::endl;
            continue;
        }

        if (!validate_exact_cache_input_n(o->n(), state.expected_n, reason)) {
            ++resume_skipped_invalid;
            std::cerr << "[resume] warning skipped_invalid_output line=" << output_lines
                      << " reason=\"" << reason << "\""
                      << std::endl;
            continue;
        }
        state.ensure_arenas(*state.expected_n);

        auto orbit = get_projective_orbit_keys(*o, *state.expected_n);
        if (!orbit.has_value()) {
            ++resume_skipped_invalid;
            std::cerr << "[resume] warning skipped_invalid_output line=" << output_lines
                      << " reason=\"failed to build exact projective orbit\""
                      << std::endl;
            continue;
        }
        ++full_orbits;

        std::string actual_pid = pid_label(*state.expected_n, orbit->p_key.hash);
        std::string actual_eid = eid_label(*state.expected_n, orbit->e_keys[0].hash);
        if (parsed.expected_pid.has_value() && *parsed.expected_pid != actual_pid) {
            std::cerr << "[resume] warning pid_mismatch line=" << output_lines
                      << " expected=" << *parsed.expected_pid
                      << " actual=" << actual_pid
                      << std::endl;
        }
        if (parsed.expected_eid.has_value() && *parsed.expected_eid != actual_eid) {
            std::cerr << "[resume] warning eid_mismatch line=" << output_lines
                      << " expected=" << *parsed.expected_eid
                      << " actual=" << actual_eid
                      << std::endl;
        }

        for (const auto& e_key : orbit->e_keys)
            state.covered_e.insert_if_absent(e_key);

        bool inserted_p = state.seen_p.insert_if_absent(orbit->p_key);
        if (!inserted_p) {
            std::cerr << "[resume] warning duplicate_p line=" << output_lines << std::endl;
        }

        ++resume_configs;
        if (progress_interval_sec > 0 && progress_timer.is_due(progress_interval_sec)) {
            progress::ProgressRate rate = progress_timer.sample(output_lines);
            std::cerr << "[resume] loaded_lines=" << output_lines
                      << " p_unique=" << state.seen_p.size()
                      << " covered_e=" << state.covered_e.size();
            progress::print_progress_rate(std::cerr, rate);
            std::cerr << std::endl;
        }
    }

    progress::ProgressRate done_rate = progress_timer.sample(output_lines);
    std::cerr << "[resume] done output=" << path
              << " lines=" << output_lines
              << " configs=" << resume_configs
              << " skipped_invalid=" << resume_skipped_invalid
              << " p_unique=" << state.seen_p.size()
              << " covered_e=" << state.covered_e.size()
              << " full_orbits=" << full_orbits
              << " output_lines=" << output_lines
              << " resume_configs=" << resume_configs
              << " resume_skipped_invalid=" << resume_skipped_invalid;
    progress::print_progress_rate(std::cerr, done_rate);
    std::cerr << std::endl;

    return 0;
}
