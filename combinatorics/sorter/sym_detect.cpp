// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Roman Parpalak and Denis Utkin

#include <cstddef>
#include <exception>
#include <iostream>
#include <optional>
#include <string>
#include <vector>

#include "argparse.hpp"

#include "exact_key.hpp"
#include "generator_parser.hpp"
#include "json_writer.hpp"
#include "line_reader.hpp"
#include "omatrix.hpp"
#include "progress.hpp"
#include "symmetry_analyzer.hpp"

namespace {

struct Params {
    std::string ifile;
    std::vector<std::string> ifiles;
    bool no_mirror = false;
    bool include_full_perms = false;
    bool include_gens = false;
    double progress = 0.0;
};

struct AnalyzerCounters {
    std::size_t processed = 0;
    std::size_t invalid = 0;
};

void warn_invalid(const std::string& path, std::size_t line_no, const std::string& reason) {
    std::cerr << "[analyzer] warning skipped_invalid_input file=" << path
              << " line=" << line_no
              << " reason=\"" << reason << "\""
              << std::endl;
}

void print_progress_line(const std::string& path,
                         std::size_t file_lines,
                         std::size_t total_lines,
                         std::size_t processed,
                         std::size_t invalid,
                         const progress::ProgressRate& rate,
                         bool done) {
    std::cerr << "[analyzer]";
    if (done)
        std::cerr << " done";
    std::cerr << " file=" << path
              << " file_lines=" << file_lines
              << " total_lines=" << total_lines
              << " processed=" << processed
              << " invalid=" << invalid;
    progress::print_progress_rate(std::cerr, rate);
    std::cerr << std::endl;
}

int process_file(const std::string& path,
                 const Params& params,
                 std::optional<std::size_t>& expected_n,
                 std::size_t& total_lines,
                 AnalyzerCounters& counters,
                 const std::string& gens_field_arg /*unused, kept for future*/) {
    (void)gens_field_arg;
    LineReader reader(path, /*follow=*/false);
    if (!reader) {
        std::cerr << "Could not open file " << path << std::endl;
        return -1;
    }

    std::string line;
    std::size_t file_lines = 0;
    progress::ProgressTimer timer;

    AnalyzerOptions options;
    options.no_mirror = params.no_mirror;

    while (reader.getline(line)) {
        ++file_lines;
        ++total_lines;

        if (params.progress > 0 && timer.is_due(params.progress)) {
            progress::ProgressRate rate = timer.sample(total_lines);
            print_progress_line(path, file_lines, total_lines,
                                counters.processed, counters.invalid, rate,
                                /*done=*/false);
        }

        GeneratorParseResult parsed = parse_generators(line);
        if (!parsed.ok) {
            ++counters.invalid;
            warn_invalid(path, file_lines, parsed.reason);
            continue;
        }
        std::vector<std::size_t> gens = parsed.gens;
        std::string reason;
        if (!validate_exact_cache_input_n(parsed.n, expected_n, reason)) {
            ++counters.invalid;
            warn_invalid(path, file_lines, reason);
            continue;
        }

        std::optional<SymmetryAnalysisResult> result;
        auto simple = get_exact_cache_OMatrix(gens, reason);
        if (!parsed.has_multiple_points && simple.has_value()) {
            result = analyze_symmetries(*simple, total_lines, options);
        } else {
            const addline::OMatrixResult built = addline::omatrix_from_word(
                parsed.word, static_cast<int>(parsed.n));
            if (!built.ok) {
                ++counters.invalid;
                warn_invalid(path, file_lines, built.reason);
                continue;
            }
            const addline::ProjectiveOMatrix form =
                addline::projective_closure(built.o);
            result = analyze_symmetries(
                built.o, form, total_lines, options);
        }
        if (!result.has_value()) {
            ++counters.invalid;
            warn_invalid(path, file_lines, "analyze_symmetries failed");
            continue;
        }

        JsonWriterOptions jopts;
        jopts.include_full_perms = params.include_full_perms;
        jopts.include_gens = params.include_gens;
        if (params.include_gens) {
            jopts.gens_string = addline::format_word(parsed.word);
        }

        std::cout << write_analysis_json(*result, jopts) << '\n';
        std::cout.flush();
        ++counters.processed;
    }

    if (params.progress > 0) {
        progress::ProgressRate rate = timer.sample(total_lines);
        print_progress_line(path, file_lines, total_lines,
                            counters.processed, counters.invalid, rate,
                            /*done=*/true);
    }
    return 0;
}

}  // namespace

int main(int argc, char** argv) {
#ifndef NDEBUG
    std::cerr << "[WARNING] Debug build - expect 5-10x slower performance. Rebuild with -DCMAKE_BUILD_TYPE=Release." << std::endl;
#endif
    argparse::ArgumentParser program("sym_detect");
    Params params;

    program.add_argument("-i", "--ifile")
            .help("input file")
            .store_into(params.ifile);

    program.add_argument("-I", "--inputs")
            .help("input files")
            .nargs(1, 100)
            .store_into(params.ifiles);

    program.add_argument("--no-mirror")
            .help("disable mirror symmetries (T = 2n oriented, instead of the default T = 4n)")
            .flag()
            .store_into(params.no_mirror);

    program.add_argument("--include-full-perms")
            .help("emit full p_sym_perm and e_sym_perm arrays in JSON")
            .flag()
            .store_into(params.include_full_perms);

    program.add_argument("--include-gens")
            .help("emit original generator word as 'gens' field in JSON")
            .flag()
            .store_into(params.include_gens);

    program.add_argument("--progress")
            .help("print progress to stderr every N seconds (0 = disabled)")
            .default_value(0.0)
            .store_into(params.progress);

    // Reject sorter-only modes with a clear error so users notice typos.
    static const char* kForbidden[] = {
        "--uniq", "-u",
        "--exact-cache",
        "--follow", "-f",
        "--resume-output", "--resume-state", "--resume-skip-lines",
        "--save-state", "--state-progress",
        "--rotate", "-r",
        "--mirror", "-m",
        "--line", "-l",
        "--stream", "-s",
        "--print", "-p",
    };

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        // Allow `--foo=value` style by trimming `=...`.
        std::size_t eq = arg.find('=');
        std::string key = (eq == std::string::npos) ? arg : arg.substr(0, eq);
        for (const char* f : kForbidden) {
            if (key == f) {
                std::cerr << "sym_detect does not support sorter flag '" << key
                          << "'. It is an analyzer, not a streaming filter." << std::endl;
                return 1;
            }
        }
    }

    try {
        program.parse_args(argc, argv);
    } catch (const std::exception& err) {
        std::cerr << err.what() << std::endl;
        std::cerr << program;
        return 1;
    }

    if (!params.ifile.empty())
        params.ifiles.push_back(params.ifile);

    if (params.ifiles.empty()) {
        std::cerr << "Input requires --ifile or --inputs" << std::endl;
        return 1;
    }

    std::optional<std::size_t> expected_n;
    std::size_t total_lines = 0;
    AnalyzerCounters counters;
    for (const auto& path : params.ifiles) {
        int rc = process_file(path, params, expected_n, total_lines, counters, "");
        if (rc != 0)
            return rc;
    }
    return 0;
}
