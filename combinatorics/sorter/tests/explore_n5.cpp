// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Roman Parpalak and Denis Utkin

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include "omatrix.hpp"
#include "symmetry_analyzer.hpp"
#include "traced_transform.hpp"
#include "line_permutation_group.hpp"

#include <iostream>
#include <set>
#include <vector>
#include <string>
#include <sstream>

static std::string perm_str(const LinePermutation& p) {
    std::ostringstream s;
    s << "[";
    auto a = p.as_array();
    for (size_t i = 0; i < a.size(); ++i) {
        if (i) s << " ";
        s << a[i];
    }
    s << "]";
    return s.str();
}

static std::string perm_key(const LinePermutation& p) {
    auto a = p.as_array();
    std::ostringstream s;
    for (size_t i = 0; i < a.size(); ++i) {
        if (i) s << ",";
        s << a[i];
    }
    return s.str();
}

// Manually enumerate (cut, rotation, mirror) -> traced perm if matrix matches.
struct Found {
    int cut;       // -1 = no projective rotation, else p in 0..n-1
    size_t rot;
    bool mirror;
    LinePermutation perm;
};

static std::vector<Found> enumerate_all(const OMatrix& M, bool include_mirror) {
    size_t n = M.n();
    std::vector<Found> out;
    auto try_cut = [&](int cut, const TracedOMatrix& base) {
        for (size_t r = 0; r < 2 * n; ++r) {
            auto rotated = traced_rotate(base, r);
            if (!rotated) continue;

            if (rotated->matrix == M) {
                Found f{cut, r, false, rotated->old_to_new_line};
                out.push_back(std::move(f));
            }
            if (include_mirror) {
                auto reflected = traced_reflect(*rotated);
                if (reflected.matrix == M) {
                    Found f{cut, r, true, reflected.old_to_new_line};
                    out.push_back(std::move(f));
                }
            }
        }
    };

    auto identity = traced_identity(M);
    try_cut(-1, identity);
    for (size_t c = 0; c < n; ++c) {
        auto sr = traced_projective_rotation(identity, c);
        try_cut(static_cast<int>(c), sr);
    }
    return out;
}

TEST_CASE("explore n=5 closure") {
    std::vector<size_t> gens = {1, 3, 2, 1, 3, 0, 1, 2, 1, 3};
    auto opt = get_OMatrix(gens);
    REQUIRE(opt.has_value());
    OMatrix M = *opt;
    size_t n = M.n();
    std::cout << "n = " << n << "\n";

    auto oriented = enumerate_all(M, false);
    auto with_mirror = enumerate_all(M, true);

    // Deduplicate oriented by perm.
    std::map<std::string, Found> oriented_map;
    for (auto& f : oriented) oriented_map[perm_key(f.perm)] = f;

    std::map<std::string, Found> mirror_map;
    for (auto& f : with_mirror) mirror_map[perm_key(f.perm)] = f;

    std::cout << "Oriented decomposition tuples (cut,rot): " << oriented.size()
              << " | distinct line perms: " << oriented_map.size() << "\n";
    std::cout << "With mirror tuples: " << with_mirror.size()
              << " | distinct line perms: " << mirror_map.size() << "\n\n";

    // Print all oriented perms.
    std::cout << "=== Oriented set (" << oriented_map.size() << " elements) ===\n";
    int idx = 0;
    for (auto& [k, f] : oriented_map) {
        std::cout << "[" << idx++ << "] " << perm_str(f.perm)
                  << "  via cut=" << f.cut << " rot=" << f.rot << "\n";
    }

    // Closure check: compose every pair, see if result is in oriented set.
    std::cout << "\n=== Closure violations ===\n";
    int total_pairs = 0, out_of_set = 0;
    int show = 0;
    for (auto& [k1, f1] : oriented_map) {
        for (auto& [k2, f2] : oriented_map) {
            ++total_pairs;
            LinePermutation prod = f1.perm.compose(f2.perm);
            std::string pk = perm_key(prod);
            if (oriented_map.count(pk) == 0) {
                ++out_of_set;
                if (show < 10) {
                    std::cout << perm_str(f1.perm) << " ∘ " << perm_str(f2.perm)
                              << " = " << perm_str(prod);
                    if (mirror_map.count(pk)) {
                        auto& mf = mirror_map[pk];
                        std::cout << "  [in mirror set: cut=" << mf.cut
                                  << " rot=" << mf.rot << " mirror=" << (mf.mirror ? "Y" : "N") << "]";
                    } else {
                        std::cout << "  [NOT EVEN IN MIRROR SET - bug?]";
                    }
                    std::cout << "\n";
                    ++show;
                }
            }
        }
    }
    std::cout << "Pairs: " << total_pairs
              << " | out-of-set: " << out_of_set
              << " (" << (100.0 * out_of_set / total_pairs) << "%)\n";

    // What does the closure give?
    std::set<std::string> seen;
    std::vector<LinePermutation> frontier;
    for (auto& [k, f] : oriented_map) {
        seen.insert(k);
        frontier.push_back(f.perm);
    }
    while (!frontier.empty()) {
        auto cur = frontier.back();
        frontier.pop_back();
        for (auto& [k, f] : oriented_map) {
            auto prod = cur.compose(f.perm);
            std::string pk = perm_key(prod);
            if (seen.insert(pk).second) {
                frontier.push_back(prod);
            }
        }
    }
    std::cout << "\nClosure of oriented set has " << seen.size() << " elements.\n";

    // Compare: is it equal to mirror set?
    bool same = (seen.size() == mirror_map.size());
    if (same) {
        for (auto& [k, _] : mirror_map) {
            if (!seen.count(k)) { same = false; break; }
        }
    }
    std::cout << "Closure equals mirror set? " << (same ? "YES" : "NO") << "\n";
}
