// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Roman Parpalak and Denis Utkin

#include "stream_drivers.hpp"

#include "configuration.hpp"
#include "exact_key.hpp"
#include "generator_parser.hpp"
#include "id_labels.hpp"
#include "line_reader.hpp"
#include "print_config.hpp"
#include "progress.hpp"

#include <iostream>
#include <optional>
#include <utility>

int stream_print_file(const std::string& path,
                      const Params& params,
                      std::size_t& total_lines,
                      bool follow) {
    LineReader reader(path, follow);
    if (!reader) {
        std::cerr << "Could not open file " << path << std::endl;
        return -1;
    }

    std::string line;
    int i = 0;
    progress::ProgressTimer progress_timer;
    while (reader.getline(line)) {
        ++i;
        ++total_lines;

        if (params.progress > 0 && progress_timer.is_due(params.progress)) {
            progress::ProgressRate rate = progress_timer.sample(total_lines);
            std::cerr << "[progress] file=" << path
                      << " file_lines=" << i
                      << " total_lines=" << total_lines;
            progress::print_progress_rate(std::cerr, rate);
            std::cerr << std::endl;
        }

        if (params.line_num > 0 && static_cast<std::size_t>(i) != params.line_num)
            continue;

        GeneratorParseResult parsed = parse_generators(line);
        if (params.line_num > 0 && !parsed.ok) {
            std::cerr << "No generators found on line " << i << " in " << path
                      << ": " << parsed.reason << std::endl;
            return -1;
        }
        if (!parsed.ok) {
            // Say what is skipped: a silent drop hides an unknown format.
            std::cerr << "[stream] warning skipped_invalid_input file=" << path
                      << " line=" << i
                      << " reason=\"" << parsed.reason << "\""
                      << std::endl;
            continue;
        }

        Configuration conf(i, parsed);
        if (!apply_output_transformations(conf, params))
            continue;
        print_configuration(conf, params);

        if (params.line_num > 0)
            break;
    }

    if (params.progress > 0) {
        progress::ProgressRate rate = progress_timer.sample(total_lines);
        std::cerr << "[progress] done file=" << path
                  << " file_lines=" << i
                  << " total_lines=" << total_lines;
        progress::print_progress_rate(std::cerr, rate);
        std::cerr << std::endl;
    }

    return 0;
}

int stream_unique_file(const std::string& path,
                       const Params& params,
                       const std::string& uniq_mode,
                       std::unordered_set<std::string>& key_set,
                       std::size_t& total_lines,
                       bool follow) {
    LineReader reader(path, follow);
    if (!reader) {
        std::cerr << "Could not open file " << path << std::endl;
        return -1;
    }

    std::string line;
    int i = 0;
    progress::ProgressTimer progress_timer;
    while (reader.getline(line)) {
        ++i;
        ++total_lines;

        if (params.progress > 0 && progress_timer.is_due(params.progress)) {
            progress::ProgressRate rate = progress_timer.sample(total_lines);
            std::cerr << "[progress] file=" << path
                      << " file_lines=" << i
                      << " total_lines=" << total_lines
                      << " unique=" << key_set.size();
            progress::print_progress_rate(std::cerr, rate);
            std::cerr << std::endl;
        }

        if (params.line_num > 0 && static_cast<std::size_t>(i) != params.line_num)
            continue;

        GeneratorParseResult parsed = parse_generators(line);
        if (params.line_num > 0 && !parsed.ok) {
            std::cerr << "No generators found on line " << i << " in " << path
                      << ": " << parsed.reason << std::endl;
            return -1;
        }
        if (!parsed.ok) {
            // Say what is skipped: a silent drop hides an unknown format.
            std::cerr << "[stream] warning skipped_invalid_input file=" << path
                      << " line=" << i
                      << " reason=\"" << parsed.reason << "\""
                      << std::endl;
            continue;
        }

        Configuration conf(i, parsed);
        const auto key = conf.get_class_key(uniq_mode);
        if (!key) continue;
        const std::string bytes(reinterpret_cast<const char*>(key->data()),
                                key->size());
        if (!key_set.insert(bytes).second) continue;
        if (!apply_output_transformations(conf, params))
            continue;
        print_configuration(conf, params);

        if (params.line_num > 0)
            break;
    }

    if (params.progress > 0) {
        progress::ProgressRate rate = progress_timer.sample(total_lines);
        std::cerr << "[progress] done file=" << path
                  << " file_lines=" << i
                  << " total_lines=" << total_lines
                  << " unique=" << key_set.size();
        progress::print_progress_rate(std::cerr, rate);
        std::cerr << std::endl;
    }

    return 0;
}

int stream_unique_file_exact_cache(const std::string& path,
                                   const Params& params,
                                   ExactCacheState& state,
                                   std::size_t& resume_skip_remaining,
                                   bool count_resume_skipped_lines,
                                   bool follow) {
    LineReader reader(path, follow);
    if (!reader) {
        std::cerr << "Could not open file " << path << std::endl;
        return -1;
    }

    std::string line;
    int i = 0;
    progress::ProgressTimer progress_timer;
    auto finish_line = [&]() -> int {
        if (params.progress > 0 && progress_timer.is_due(params.progress)) {
            progress::ProgressRate rate = progress_timer.sample(state.counters.total_lines);
            print_exact_cache_progress(path, i, state, false, rate);
        }
        if (!params.save_state.empty() && params.state_progress > 0 &&
            state.counters.total_lines % params.state_progress == 0) {
            return save_exact_cache_state(params.save_state, state);
        }
        return 0;
    };

    while (reader.getline(line)) {
        ++i;

        if (resume_skip_remaining > 0) {
            --resume_skip_remaining;
            if (count_resume_skipped_lines)
                ++state.counters.total_lines;
            if (resume_skip_remaining == 0) {
                progress_timer.sample(state.counters.total_lines);
                std::cerr << "[resume] skipped file=" << path
                          << " file_lines=" << i
                          << " total_skipped=" << params.resume_skip_lines
                          << " next_line=" << (i + 1)
                          << std::endl;
            }
            continue;
        }

        ++state.counters.total_lines;

        if (params.line_num > 0 && static_cast<std::size_t>(i) != params.line_num)
            continue;

        GeneratorParseResult parsed = parse_generators(line);
        if (!parsed.ok) {
            std::cerr << "[stream] warning skipped_invalid_input file=" << path
                      << " line=" << i
                      << " reason=\"" << parsed.reason << "\""
                      << std::endl;
            if (finish_line() != 0) return -1;
            continue;
        }

        if (parsed.has_multiple_points) {
            std::cerr << "[stream] warning skipped_invalid_input file=" << path
                      << " line=" << i
                      << " reason=\"--exact-cache does not yet support multiple points; use --uniq P without --exact-cache\""
                      << std::endl;
            if (finish_line() != 0) return -1;
            continue;
        }

        std::vector<std::size_t> gens = std::move(parsed.gens);
        std::string reason;
        auto o = get_exact_cache_OMatrix(gens, reason);
        if (!o.has_value()) {
            std::cerr << "[stream] warning skipped_invalid_input file=" << path
                      << " line=" << i
                      << " reason=\"" << reason << "\""
                      << std::endl;
            if (finish_line() != 0) return -1;
            continue;
        }

        if (!validate_exact_cache_input_n(o->n(), state.expected_n, reason)) {
            std::cerr << "[stream] warning skipped_invalid_input file=" << path
                      << " line=" << i
                      << " reason=\"" << reason << "\""
                      << std::endl;
            if (finish_line() != 0) return -1;
            continue;
        }
        state.ensure_arenas(*state.expected_n);

        auto e0 = exact_min_o_key(*o, *state.expected_n);
        if (!e0.has_value()) {
            std::cerr << "[stream] warning skipped_invalid_input file=" << path
                      << " line=" << i
                      << " reason=\"failed to build exact E canonical key\""
                      << std::endl;
            if (finish_line() != 0) return -1;
            continue;
        }

        if (state.covered_e.contains_and_mark(e0->key)) {
            if (finish_line() != 0) return -1;
            continue;
        }

        auto orbit = get_projective_orbit_keys(*o, *state.expected_n, std::move(*e0));
        if (!orbit.has_value()) {
            std::cerr << "[stream] warning skipped_invalid_input file=" << path
                      << " line=" << i
                      << " reason=\"failed to build exact projective orbit\""
                      << std::endl;
            if (finish_line() != 0) return -1;
            continue;
        }

        for (const auto& e_key : orbit->e_keys) {
            state.covered_e.insert_if_absent(e_key);
        }
        // Mark the input's own E-class as hit (first in orbit's e_keys).
        state.covered_e.contains_and_mark(orbit->e_keys[0]);

        bool inserted_p = state.seen_p.insert_if_absent(orbit->p_key);
        if (!inserted_p) {
            if (finish_line() != 0) return -1;
            continue;
        }

        Configuration conf(static_cast<int>(state.counters.total_lines), gens);
        conf.o = *o;
        StreamPrintOverrides overrides;
        overrides.eid = eid_label(*state.expected_n, orbit->e_keys[0].hash);
        overrides.pid = pid_label(*state.expected_n, orbit->p_key.hash);

        if (!apply_output_transformations(conf, params))
            continue;
        print_configuration(conf, params, &overrides);
        std::cout.flush();

        if (finish_line() != 0) return -1;
        if (params.line_num > 0) break;
    }

    if (params.progress > 0) {
        progress::ProgressRate rate = progress_timer.sample(state.counters.total_lines);
        print_exact_cache_progress(path, i, state, true, rate);
    }

    if (resume_skip_remaining > 0) {
        progress_timer.sample(state.counters.total_lines);
        std::cerr << "[resume] skipped file=" << path
                  << " file_lines=" << i
                  << " total_skipped=" << (params.resume_skip_lines - resume_skip_remaining)
                  << std::endl;
    }

    return 0;
}
