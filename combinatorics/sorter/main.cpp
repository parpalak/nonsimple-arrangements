// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Roman Parpalak and Denis Utkin

#include <iostream>
#include <unordered_set>

#include "omatrix.hpp"
#include "params.hpp"
#include "print_config.hpp"
#include "resume.hpp"
#include "state_io.hpp"
#include "stream_drivers.hpp"

#include "argparse.hpp"


int main(int argc, char **argv) {
#ifndef NDEBUG
    std::cerr << "[WARNING] Debug build - expect 5-10x slower performance. Rebuild with -DCMAKE_BUILD_TYPE=Release." << std::endl;
#endif
    argparse::ArgumentParser program("sorter");
    Params params;

    program.add_argument("-i", "--ifile")
            .help("input file")
            .store_into(params.ifile);

    program.add_argument("-I", "--inputs")
            .help("input files")
            .nargs(1, 4096)
            .store_into(params.ifiles);

    program.add_argument("-l", "--line")
            .help("line number (starts from 1)")
            .store_into(params.line_num);

    program.add_argument("-r", "--rotate")
            .help("rotate configuration")
            .store_into(params.rotate);

    program.add_argument("-m", "--mirror")
            .help("mirror configuration")
            .flag()
            .store_into(params.mirror);

    program.add_argument("-f", "--follow")
            .help("follow input file as it grows (like tail -f)")
            .flag()
            .store_into(params.follow);

    program.add_argument("--exact-cache")
            .help("use exact E-cache acceleration for --uniq P")
            .flag()
            .store_into(params.exact_cache);

    program.add_argument("--resume-output")
            .help("output file from a previous exact-cache stream run")
            .store_into(params.resume_output);

    program.add_argument("--resume-state")
            .help("binary exact-cache state from a previous --save-state run")
            .store_into(params.resume_state);

    program.add_argument("--save-state")
            .help("write binary exact-cache state after processing")
            .store_into(params.save_state);

    program.add_argument("--state-progress")
            .help("with --save-state, write binary exact-cache state every N processed lines (0 = only at end of run)")
            .store_into(params.state_progress);

    program.add_argument("--resume-skip-lines")
            .help("number of input lines already processed before resume")
            .store_into(params.resume_skip_lines);

    program.add_argument("--progress")
            .help("print progress to stderr every N seconds (0 = disabled)")
            .default_value(0.0)
            .store_into(params.progress);

    program.add_argument("-u", "--uniq")
            .help("filter unique configurations by class: D (diagram, up to commutation), E (affine) or P (projective)")
            .store_into(params.uniq_mode);

    program.add_argument("-p", "--print")
            .help("print (gens, omatrix, n, k, A, pos, wire, wire_condensed, did, eid, pid, perm, w0, w0_reason, lf; unknown tokens are printed as literals)")
            .nargs(1, 20)
            .store_into(params.print);

    try {
        program.parse_args(argc, argv);
    }
    catch (const std::exception& err) {
        std::cerr << err.what() << std::endl;
        std::cerr << program;
        return 1;
    }

    if (!params.ifile.empty()) {
        params.ifiles.push_back(params.ifile);
    }

    std::string uniq_mode;
    if (!params.uniq_mode.empty()) {
        uniq_mode = normalize_uniq_mode(params.uniq_mode);
        if (uniq_mode != "E" && uniq_mode != "P" && uniq_mode != "D") {
            std::cerr << "Invalid --uniq mode: " << params.uniq_mode << ". Expected D, E or P." << std::endl;
            return 1;
        }
    }

    if (params.exact_cache && uniq_mode != "P") {
        std::cerr << "--exact-cache requires --uniq P" << std::endl;
        return 1;
    }

    if (params.exact_cache && params.rotate > 0) {
        std::cerr << "--rotate is not supported with --exact-cache in the first implementation" << std::endl;
        return 1;
    }

    if ((!params.resume_output.empty() || !params.resume_state.empty() || !params.save_state.empty() || params.resume_skip_lines > 0 || params.state_progress > 0) && !params.exact_cache) {
        std::cerr << "--resume-output, --resume-state, --save-state, --state-progress and --resume-skip-lines require --uniq P --exact-cache" << std::endl;
        return 1;
    }

    if (params.exact_cache && !params.resume_output.empty() && !params.resume_state.empty()) {
        std::cerr << "--resume-output and --resume-state are mutually exclusive" << std::endl;
        return 1;
    }

    if (params.exact_cache && params.resume_skip_lines > 0 && params.resume_output.empty() && params.resume_state.empty()) {
        std::cerr << "--resume-skip-lines requires --resume-output or --resume-state" << std::endl;
        return 1;
    }

    if (params.exact_cache && params.state_progress > 0 && params.save_state.empty()) {
        std::cerr << "--state-progress requires --save-state" << std::endl;
        return 1;
    }

    if (params.exact_cache && (!params.resume_output.empty() || !params.resume_state.empty() || params.resume_skip_lines > 0) && params.line_num > 0) {
        std::cerr << "--line is not supported with --resume-output, --resume-state or --resume-skip-lines" << std::endl;
        return 1;
    }

    if (params.ifiles.empty()) {
        std::cerr << "Input requires --ifile or --inputs" << std::endl;
        return 1;
    }

    if (params.exact_cache) {
        ExactCacheState state;
        bool loaded_binary_state = false;
        if (!params.resume_state.empty()) {
            int result = load_exact_cache_state(params.resume_state, state);
            if (result != 0)
                return result;
            loaded_binary_state = true;
            if (params.resume_skip_lines > 0 && params.resume_skip_lines != state.counters.total_lines) {
                std::cerr << "--resume-skip-lines does not match --resume-state total_lines: got "
                          << params.resume_skip_lines
                          << " expected " << state.counters.total_lines
                          << std::endl;
                return 1;
            }
            if (params.resume_skip_lines == 0)
                params.resume_skip_lines = state.counters.total_lines;
        }
        if (!params.resume_output.empty()) {
            int result = load_exact_cache_resume_output(params.resume_output, params.resume_skip_lines, params.progress, state);
            if (result != 0)
                return result;
        }

        size_t resume_skip_remaining = params.resume_skip_lines;
        for (size_t fi = 0; fi < params.ifiles.size(); ++fi) {
            bool follow = params.follow && (fi + 1 == params.ifiles.size());
            int result = stream_unique_file_exact_cache(params.ifiles[fi], params, state, resume_skip_remaining, !loaded_binary_state, follow);
            if (result != 0)
                return result;
        }
        if (resume_skip_remaining > 0) {
            std::cerr << "[resume] warning skip_lines_exceeds_input remaining=" << resume_skip_remaining << std::endl;
        }
        if (!params.save_state.empty()) {
            int result = save_exact_cache_state(params.save_state, state);
            if (result != 0)
                return result;
        }

        return 0;
    }

    size_t total_lines = 0;
    if (!uniq_mode.empty()) {
        std::unordered_set<std::string> key_set;
        for (size_t fi = 0; fi < params.ifiles.size(); ++fi) {
            bool follow = params.follow && (fi + 1 == params.ifiles.size());
            int result = stream_unique_file(params.ifiles[fi], params, uniq_mode, key_set, total_lines, follow);
            if (result != 0)
                return result;
        }
        return 0;
    }

    for (size_t fi = 0; fi < params.ifiles.size(); ++fi) {
        bool follow = params.follow && (fi + 1 == params.ifiles.size());
        int result = stream_print_file(params.ifiles[fi], params, total_lines, follow);
        if (result != 0)
            return result;
    }

    return 0;
}
