// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Roman Parpalak and Denis Utkin

#include <chrono>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <set>
#include <string>
#include <vector>

#include "collapse.hpp"
#include "counts.hpp"
#include "emit.hpp"
#include "route.hpp"

using namespace addline;

namespace {

enum class Source { Direct, Collapse, Both };

struct Options {
    Source source = Source::Both;
    int n = 0;
    int target = -1;
    bool first_only = false;
    bool ceiling_only = false;
    bool quiet = false;
    bool anchor_infinity = false;
    std::string input_file;
};

int default_target(int n) {
    const int m = n + 1;
    return m * (m - 2) / 3 - m / 6;
}

void usage(const char* self) {
    std::fprintf(stderr,
        "Usage: %s -n N [-i FILE] [-source S] [-target T] [-first] [-ceiling]\n"
        "       [-anchor A] [-q]\n"
        "\n"
        "Adds one line to an arrangement of n lines, n odd, and prints the\n"
        "words of n+1 lines that reach the target number of triangles.\n"
        "\n"
        "  -n N        lines in the input, odd\n"
        "  -i FILE     read FILE instead of stdin (also --ifile)\n"
        "  -source S   augmentation template: direct, collapse, or both\n"
        "              (default); the two give almost disjoint outputs\n"
        "  -target T   triangles wanted (default %s)\n"
        "  -first      print only the first route of each input\n"
        "  -ceiling    compute the upper bound only, print no words\n"
        "  -anchor A   cut (default) searches only the routes the template\n"
        "              needs; infinity searches all of them, slower, for\n"
        "              cross-checks\n"
        "  -q          no progress line on stderr\n",
        self, "floor((n+1)(n-1)/3) - floor((n+1)/6)");
}

struct Counters {
    long words = 0;
    long verdict[6] = {0, 0, 0, 0, 0, 0};
    long emitted = 0;
    long emit_failed = 0;
    std::string emit_reason;
    long other_chart = 0;
    long with_parallels = 0;
    long repeats = 0;
    long from_direct = 0, from_collapse = 0;

    void bump(Verdict v) { ++verdict[static_cast<int>(v)]; }

    void print(double seconds) const {
        std::fprintf(stderr,
            "\r%ld words %.0f/s | ok %ld no-route %ld no-quad %ld no-pentagon %ld "
            "defects-apart %ld no-defects %ld | words out %ld (direct %ld, "
            "collapse %ld)",
            words, seconds > 0 ? words / seconds : 0.0,
            verdict[static_cast<int>(Verdict::Ok)],
            verdict[static_cast<int>(Verdict::NoRoute)],
            verdict[static_cast<int>(Verdict::NoQuad)],
            verdict[static_cast<int>(Verdict::NoPentagon)],
            verdict[static_cast<int>(Verdict::DefectsApart)],
            verdict[static_cast<int>(Verdict::NoDefects)],
            emitted, from_direct, from_collapse);
        std::fflush(stderr);
    }
};

}  // namespace

int main(int argc, char** argv) {
    Options options;
    for (int i = 1; i < argc; ++i) {
        const std::string flag = argv[i];
        if (flag == "-n" && i + 1 < argc) {
            options.n = std::atoi(argv[++i]);
        } else if ((flag == "-i" || flag == "--ifile") && i + 1 < argc) {
            options.input_file = argv[++i];
        } else if (flag == "-target" && i + 1 < argc) {
            options.target = std::atoi(argv[++i]);
        } else if (flag == "-first") {
            options.first_only = true;
        } else if (flag == "-ceiling") {
            options.ceiling_only = true;
        } else if (flag == "-source" && i + 1 < argc) {
            const std::string mode = argv[++i];
            if (mode == "direct") options.source = Source::Direct;
            else if (mode == "collapse") options.source = Source::Collapse;
            else if (mode == "both") options.source = Source::Both;
            else { usage(argv[0]); return 2; }
        } else if (flag == "-anchor" && i + 1 < argc) {
            const std::string mode = argv[++i];
            if (mode == "infinity") {
                options.anchor_infinity = true;
            } else if (mode != "cut") {
                usage(argv[0]);
                return 2;
            }
        } else if (flag == "-q") {
            options.quiet = true;
        } else {
            usage(argv[0]);
            return 2;
        }
    }
    if (options.n < 3 || options.n % 2 == 0) {
        usage(argv[0]);
        return 2;
    }
    if (options.n + 1 > 64) {
        std::fprintf(stderr, "n = %d: lines are kept in 64-bit masks, n + 1 <= 64\n",
                     options.n);
        return 2;
    }
    if (options.target < 0) options.target = default_target(options.n);

    std::ifstream input_file;
    std::istream* input = &std::cin;
    if (!options.input_file.empty() && options.input_file != "-") {
        input_file.open(options.input_file);
        if (!input_file) {
            std::fprintf(stderr, "Cannot open input file: %s\n",
                         options.input_file.c_str());
            return 1;
        }
        input = &input_file;
    }

    Counters counters;
    const auto started = std::chrono::steady_clock::now();
    auto elapsed = [&] {
        return std::chrono::duration<double>(std::chrono::steady_clock::now() -
                                             started)
            .count();
    };

    // Words of the current seed: a route is found once per end of its chord.
    std::set<std::string> seen;

    // STAR: the deficient star to anchor on, -1 for none.
    auto harvest = [&](const Closure& c, int star, const char* source) {
        RouteEngine engine(c);
        if (!options.anchor_infinity) {
            if (star >= 0) {
                engine.anchor_on_star_cuts(star);
            } else {
                // No deficient star here, but on every seed measured a route
                // to the target runs through both defects, so either serves.
                const std::vector<int> defects = black_defects(c);
                if (defects.size() == 2) engine.anchor_on(defects[0]);
            }
        }
        const RouteEngine::Ceiling bound = engine.ceiling();
        if (!bound.any_route ||
            bound.per_chart[bound.best_chart()] < options.target) {
            return false;
        }
        if (options.ceiling_only) return true;

        bool any = false;
        const RouteEngine::SearchResult search =
            engine.search(options.target, !options.first_only);
        for (const RouteEngine::Witness& witness : search.witnesses) {
            const EmitResult emitted =
                word_in_witness_chart(c, options.n, witness, options.target);
            if (!emitted.ok) {
                // Accepted but not writable in any chart: keep the reason.
                ++counters.emit_failed;
                counters.emit_reason = emitted.reason;
                continue;
            }
            const std::string text = format_word(emitted.word);
            if (!seen.insert(text).second) {
                ++counters.repeats;
                continue;
            }
            if (emitted.chart != c.infinity) ++counters.other_chart;
            if (emitted.parallels) ++counters.with_parallels;
            any = true;
            ++counters.emitted;
            (star >= 0 ? counters.from_collapse : counters.from_direct)++;
            std::printf("#%ld K=%d *int=%d *ext=%d par=%d "
                        "chart=%d src=%s "
                        "did=%s eid=%s pid=%s ) %s\n",
                        counters.words, emitted.triangles, emitted.stars,
                        emitted.multiple_at_infinity, emitted.parallels,
                        emitted.chart, source, emitted.did.c_str(),
                        emitted.eid.c_str(), emitted.pid.c_str(), text.c_str());
        }
        return any;
    };

    std::string line;
    while (std::getline(*input, line)) {
        ParseResult parsed = parse_word(line);
        if (!parsed.ok) continue;
        ++counters.words;
        seen.clear();

        ClosureResult built = build_closure(parsed.word, options.n);
        if (!built.ok) {
            counters.bump(Verdict::NoDefects);
            continue;
        }
        const Closure& closure = built.closure;

        bool found_any = false;
        if (options.source != Source::Collapse) {
            found_any = harvest(closure, -1, "direct");
        }

        if (options.source == Source::Direct) {
            counters.bump(found_any ? Verdict::Ok : Verdict::NoRoute);
        } else if (!(found_any && options.first_only)) {
            // Verdicts describe the collapse branch whatever direct found.
            Verdict verdict = Verdict::NoRoute;
            const std::vector<int> defects = black_defects(closure);
            if (defects.size() != 2) {
                verdict = Verdict::NoDefects;
            } else if (!faces_share_vertex(closure, defects[0], defects[1])) {
                verdict = Verdict::DefectsApart;
            } else {
                const std::vector<CollapseCandidate> candidates =
                    collapse_candidates(closure, defects);
                if (candidates.empty()) {
                    verdict = Verdict::NoPentagon;
                } else {
                    bool usable = false, from_cores = false;
                    for (const CollapseCandidate& candidate : candidates) {
                        CoreResult core = contract(closure, candidate);
                        if (!core.ok || !core.core.usable) continue;
                        usable = true;
                        if (harvest(core.core.closure,
                                    core.core.deficient_star, "collapse")) {
                            from_cores = true;
                            if (options.first_only) break;
                        }
                    }
                    found_any = found_any || from_cores;
                    if (!usable) verdict = Verdict::NoQuad;
                    else if (from_cores) verdict = Verdict::Ok;
                }
            }
            counters.bump(found_any ? Verdict::Ok : verdict);
        } else {
            counters.bump(Verdict::Ok);
        }

        if (!options.quiet && counters.words % 64 == 0) counters.print(elapsed());
    }

    if (!options.quiet) {
        counters.print(elapsed());
        std::fprintf(stderr, "\n");
        std::fprintf(stderr, "words rotated into another chart: %ld\n",
                     counters.other_chart);
        if (counters.with_parallels) {
            std::fprintf(stderr, "words with parallel lines: %ld\n",
                         counters.with_parallels);
        }
        if (counters.repeats) {
            std::fprintf(stderr, "words found twice, one per end of the chord: %ld\n",
                         counters.repeats);
        }
        if (counters.emit_failed) {
            std::fprintf(stderr, "routes without a word: %ld (last: %s)\n",
                         counters.emit_failed, counters.emit_reason.c_str());
        }
    }
    return 0;
}
