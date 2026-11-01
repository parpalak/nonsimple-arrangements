// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Roman Parpalak and Denis Utkin

#include "symmetry_analyzer.hpp"

#include "fixed_omatrix.hpp"
#include "id_labels.hpp"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <set>
#include <sstream>
#include <string>
#include <vector>

namespace {

// Reclassify a Euclidean stabilizer H_E that lives inside the dihedral
// D_{2n} of one affine chart (arXiv:2607.29236, Definition 5.11). With
// no reflections H_E is cyclic by construction (a subgroup of the
// rotation part C_n); with at least one reflection it is dihedral
// D_k, k = |H_E|/2. The fallback_type argument carries the generic
// isomorphism-type name supplied by the caller (used for the cyclic
// branch and as a safety net).
std::string classify_euclidean_stabilizer(size_t order,
                                          size_t reflection_fixes,
                                          const std::string& fallback_type) {
    if (order == 1)
        return "C1";

    // Pure rotational stabilizer: cyclic.
    if (reflection_fixes == 0)
        return fallback_type;

    // Has at least one reflection: dihedral subgroup of D_{2n}.
    if (order % 2 != 0) {
        // Stabilizer with reflections has even order; defensive fallback.
        return fallback_type;
    }
    const size_t k = order / 2;
    return "D" + std::to_string(k);
}

// rep(E) = |orbit| / |H_E| (arXiv:2607.29236, Lemma 5.2). ORBIT_ORDER is 4n for
// a full chart and 4m when parallel lines leave only m directions, half of
// that without reflections. At odd n the search keeps one word of every mirror
// pair (Lemma 5.13 there), so the count is halved; at even n it is not.
struct RepresentativeCount {
    size_t value = 0;
    size_t orbit = 0;      // numerator actually used, for the cross-check
    bool defined = false;
};

RepresentativeCount representative_count(size_t n, size_t orbit_order,
                                         size_t stabilizer_order,
                                         bool no_mirror) {
    RepresentativeCount out;
    out.orbit = (n % 2 == 1 && !no_mirror) ? orbit_order / 2 : orbit_order;
    if (stabilizer_order == 0 || out.orbit % stabilizer_order != 0)
        return out;
    out.value = out.orbit / stabilizer_order;
    out.defined = true;
    return out;
}

bool histograms_equal(const std::map<size_t, size_t>& a,
                      const std::map<size_t, size_t>& b) {
    if (a.size() != b.size())
        return false;
    for (auto it = a.begin(), jt = b.begin(); it != a.end(); ++it, ++jt) {
        if (it->first != jt->first || it->second != jt->second)
            return false;
    }
    return true;
}

// 4. Consistency checks, shared by both analyzers. KERNEL_MUST_BE_TRIVIAL is
// off for the generalized one, where rho^n may fix a point block.
void run_consistency_checks(SymmetryAnalysisResult& result, size_t pos,
                            bool kernel_collapse_free,
                            bool kernel_must_be_trivial) {
    const size_t n = result.n;
    const std::vector<size_t>& cut_to_e = result.cut_to_e_class;
    const size_t num_classes = result.e_classes.size();

    bool any_failed = false;
    auto warn = [&](const std::string& field, const std::string& reason) {
        std::cerr << "[analyzer] check_failed pos=" << pos
                  << " field=" << field << " reason=" << reason << "\n";
        any_failed = true;
    };

    // sum_m_ok
    {
        size_t sum = 0;
        for (const auto& ec : result.e_classes)
            sum += ec.cut_count;
        result.checks.sum_m_ok = (sum == n + 1);
        if (!result.checks.sum_m_ok)
            warn("sum_m_ok", "sum=" + std::to_string(sum));
    }

    // orbit_stabilizer_ok
    {
        bool ok = true;
        for (const auto& ec : result.e_classes) {
            if (ec.cut_count * ec.stabilizer_image_order != result.p_sym_ord) {
                ok = false;
                warn("orbit_stabilizer_ok", "class=" + std::to_string(ec.local_e_id));
                break;
            }
        }
        result.checks.orbit_stabilizer_ok = ok;
    }

    // representatives_ok: representatives * stabilizer_order must recover the
    // orbit the count came from.
    {
        bool ok = true;
        for (const auto& ec : result.e_classes) {
            if (!ec.representatives_defined) {
                result.checks.representatives_applicable = false;
                continue;
            }
            if (ec.representatives * ec.stabilizer_order != ec.representatives_orbit) {
                ok = false;
                warn("representatives_ok", "class=" + std::to_string(ec.local_e_id));
                break;
            }
        }
        result.checks.representatives_ok = ok;
    }

    // generators_close: closure of p_sym_generators must equal p_sym_ord with same histogram
    {
        bool ok = true;
        auto closure = LinePermutationGroup::closure(result.p_sym_generators, n + 1);
        if (closure.size() != result.p_sym_ord) {
            ok = false;
            warn("generators_close", "closure_size=" + std::to_string(closure.size()) +
                 " expected=" + std::to_string(result.p_sym_ord));
        }
        if (ok) {
            std::map<size_t, size_t> hist;
            for (const auto& e : closure)
                hist[e.order()]++;
            if (!histograms_equal(hist, result.p_sym_element_orders)) {
                ok = false;
                warn("generators_close", "histogram mismatch");
            }
        }
        result.checks.generators_close = ok;
    }

    // e_generators_close
    {
        bool ok = true;
        for (const auto& ec : result.e_classes) {
            auto cl = LinePermutationGroup::closure(ec.e_sym_generators, n + 1);
            if (cl.size() != ec.stabilizer_image_order) {
                ok = false;
                warn("e_generators_close", "class=" + std::to_string(ec.local_e_id) +
                     " closure_size=" + std::to_string(cl.size()) +
                     " expected=" + std::to_string(ec.stabilizer_image_order));
                break;
            }
        }
        result.checks.e_generators_close = ok;
    }

    // chart_to_e_ok
    {
        bool ok = true;
        for (size_t v : cut_to_e) {
            if (v >= num_classes) {
                ok = false;
                break;
            }
        }
        result.checks.chart_to_e_ok = ok;
        if (!ok)
            warn("chart_to_e_ok", "out of range");
    }

    // kernel_collapse_free: the symmetry enumeration of steps 2 and 4 never
    // deduplicated two matches onto one line permutation (see the note where
    // the flag is computed). A failure would mean rho^n -- or some other
    // kernel element -- fixed an O-matrix, contradicting arXiv:2607.29236, Lemma C.2.
    {
        result.checks.kernel_collapse_free = kernel_collapse_free;
        if (!kernel_collapse_free && kernel_must_be_trivial)
            warn("kernel_collapse_free", "distinct matches collapsed onto one line permutation");
    }

    (void)any_failed;
}

}  // namespace

std::optional<SymmetryAnalysisResult> analyze_symmetries(
    const OMatrix& matrix,
    size_t pos,
    const AnalyzerOptions& options) {
    const size_t n = matrix.n();
    if (n == 0)
        return std::nullopt;

    SymmetryAnalysisResult result;
    result.pos = pos;
    result.n = n;

    // Flat-buffer view of the original (closed) matrix; comparing rotated
    // candidates becomes a single memcmp on n*(n-1) bytes instead of a full
    // OMatrix construction + nested-vector equality.
    auto fixed_orig = FixedOMatrix::from_omatrix(matrix);
    if (!fixed_orig.has_value())
        return std::nullopt;
    const size_t key_len = fixed_orig->key_len();
    const uint8_t* original_bytes = fixed_orig->data();

    // 1. Compute post-cut OMatrices, their flat buffers, and the projective-
    // step permutations. Slot `n` is the original (no projective step).
    std::vector<OMatrix> post_cut(n + 1);
    std::vector<FixedOMatrix> state_fixed(n + 1);
    std::vector<LinePermutation> chart_perms(n + 1, LinePermutation::identity(n + 1));
    std::vector<ExactMinOKey> per_cut_keys(n + 1);

    TracedOMatrix identity_traced = traced_identity(matrix);

    post_cut[n] = matrix;
    state_fixed[n] = *fixed_orig;
    // chart_perms[n] is already identity (no projective step for the original chart).
    {
        auto k = exact_min_o_key(matrix, n);
        if (!k.has_value())
            return std::nullopt;
        per_cut_keys[n] = std::move(*k);
    }

    for (size_t cut = 0; cut < n; ++cut) {
        auto pr = matrix.projective_rotate(cut);
        if (!pr.has_value())
            return std::nullopt;
        post_cut[cut] = *pr;
        auto fp = FixedOMatrix::from_omatrix(*pr);
        if (!fp.has_value())
            return std::nullopt;
        state_fixed[cut] = std::move(*fp);

        TracedOMatrix sr = traced_projective_rotation(identity_traced, cut);
        chart_perms[cut] = sr.old_to_new_line;

        auto k = exact_min_o_key(*pr, n);
        if (!k.has_value())
            return std::nullopt;
        per_cut_keys[cut] = std::move(*k);
    }

    // P-class identifier: hash of the lexicographically minimal E-key across
    // the projective orbit (the cuts plus the original chart in slot n). This
    // mirrors get_projective_orbit_keys: p_key = lex-min over the orbit, pid =
    // hash(p_key). per_cut_keys already hold the per-chart canonical keys.
    size_t p_cut = n;
    for (size_t cut = 0; cut < n; ++cut) {
        if (compact_key_less(per_cut_keys[cut].key, per_cut_keys[p_cut].key))
            p_cut = cut;
    }
    result.pid = pid_label(n, per_cut_keys[p_cut].key.hash);

    // Precomputed line-permutations for the 2n euclidean rotations and the
    // single mirror, both acting on lines 0..n with n fixed (projective
    // infinity stays at n).
    std::vector<LinePermutation> rot_perms;
    rot_perms.reserve(2 * n);
    {
        const size_t two_n = 2 * n;
        for (size_t r = 0; r < two_n; ++r) {
            std::vector<size_t> v(n + 1);
            for (size_t line = 0; line < n; ++line)
                v[line] = (line >= r) ? (line - r) : ((line + two_n - r) % n);
            v[n] = n;
            rot_perms.emplace_back(std::move(v));
        }
    }
    LinePermutation mirror_perm = [&]() {
        std::vector<size_t> v(n + 1);
        for (size_t k = 0; k < n; ++k) v[k] = n - 1 - k;
        v[n] = n;
        return LinePermutation(std::move(v));
    }();

    // 2. P-class symmetry enumeration on flat byte buffers. Reusable scratch
    // buffers; full OMatrix is never built per iteration.
    //
    // kernel_collapse_free: every match below contributes a distinct line
    // permutation, so the std::set never merges two entries. This holds
    // because rho^n (the only nontrivial element of ker tau) reverses every
    // row and fixes no O-matrix (arXiv:2607.29236, Lemma C.2), so no two matching rotations
    // r, r+n coincide. gp_match_count counts raw matches; we compare it with
    // seen_perms.size() in the consistency checks below.
    bool kernel_collapse_free = true;
    size_t gp_match_count = 0;
    std::set<LinePermutation> seen_perms;
    std::vector<uint8_t> scratch(key_len);
    std::vector<uint8_t> mirror_scratch(key_len);

    for (size_t s = 0; s < state_fixed.size(); ++s) {
        const FixedOMatrix& st = state_fixed[s];
        const LinePermutation& chart_perm = chart_perms[s];
        for (size_t r = 0; r < 2 * n; ++r) {
            fixed_o::rotate(st.data(), n, r, scratch.data());
            bool match_rot = std::memcmp(scratch.data(), original_bytes, key_len) == 0;
            bool match_refl = false;
            if (!options.no_mirror) {
                fixed_o::mirror(scratch.data(), n, mirror_scratch.data());
                match_refl = std::memcmp(mirror_scratch.data(), original_bytes, key_len) == 0;
            }
            if (!match_rot && !match_refl)
                continue;

            LinePermutation rot_full = chart_perm.compose(rot_perms[r]);
            if (match_rot) {
                seen_perms.insert(rot_full);
                ++gp_match_count;
            }
            if (match_refl) {
                seen_perms.insert(rot_full.compose(mirror_perm));
                ++gp_match_count;
            }
        }
    }
    kernel_collapse_free = kernel_collapse_free && (gp_match_count == seen_perms.size());

    std::vector<LinePermutation> p_elements(seen_perms.begin(), seen_perms.end());
    if (p_elements.empty())
        p_elements.push_back(LinePermutation::identity(n + 1));  // safety net

    LinePermutationGroup p_group = LinePermutationGroup::from_elements(p_elements);
    result.p_sym_ord = p_group.order();
    result.p_sym_type = p_group.type();
    result.p_sym_generators = p_group.generators();
    result.p_sym_elements = p_group.elements();
    result.p_sym_element_orders = p_group.element_orders();

    // Local E-class assignment: cut=n => 0; then cuts 0..n-1 in order of first
    // appearance receive consecutive ids.
    auto key_to_string = [](const CompactKey& k) {
        return std::string(reinterpret_cast<const char*>(k.bytes.data()), k.bytes.size());
    };

    std::vector<size_t> cut_to_e(n + 1, 0);
    std::vector<std::string> id_to_key;
    id_to_key.push_back(key_to_string(per_cut_keys[n].key));
    cut_to_e[n] = 0;

    auto find_or_insert = [&](const std::string& s) -> size_t {
        for (size_t i = 0; i < id_to_key.size(); ++i)
            if (id_to_key[i] == s)
                return i;
        id_to_key.push_back(s);
        return id_to_key.size() - 1;
    };

    for (size_t cut = 0; cut < n; ++cut) {
        cut_to_e[cut] = find_or_insert(key_to_string(per_cut_keys[cut].key));
    }
    result.cut_to_e_class = cut_to_e;

    // 3. Build per-class info.
    const size_t num_classes = id_to_key.size();
    result.e_classes.resize(num_classes);

    // Pick a representative cut for each class and aggregate cuts.
    std::vector<long> first_cut_for_class(num_classes, -1);
    std::vector<size_t> class_cut_count(num_classes, 0);
    // Always make cut=n the representative for class 0 (the "mother" E-class).
    first_cut_for_class[0] = static_cast<long>(n);
    for (size_t cut = 0; cut <= n; ++cut) {
        size_t id = cut_to_e[cut];
        class_cut_count[id]++;
        if (first_cut_for_class[id] < 0)
            first_cut_for_class[id] = static_cast<long>(cut);
    }

    for (size_t id = 0; id < num_classes; ++id) {
        ELocalClass& ec = result.e_classes[id];
        ec.local_e_id = id;
        ec.cut_count = class_cut_count[id];
        ec.eid = eid_label(n, per_cut_keys[static_cast<size_t>(first_cut_for_class[id])].key.hash);

        // Stabilizer: enumerate euclidean transformations of the post-cut
        // representative against itself, on flat byte buffers.
        const FixedOMatrix& rep_fixed = state_fixed[static_cast<size_t>(first_cut_for_class[id])];
        const uint8_t* rep_bytes = rep_fixed.data();

        size_t rot_fixes = 0;
        size_t refl_fixes = 0;
        size_t stab_match_count = 0;  // raw matches before deduplication
        std::set<LinePermutation> stab_perms;

        for (size_t r = 0; r < 2 * n; ++r) {
            fixed_o::rotate(rep_bytes, n, r, scratch.data());
            bool match_rot = std::memcmp(scratch.data(), rep_bytes, key_len) == 0;
            bool match_refl = false;
            if (!options.no_mirror) {
                fixed_o::mirror(scratch.data(), n, mirror_scratch.data());
                match_refl = std::memcmp(mirror_scratch.data(), rep_bytes, key_len) == 0;
            }
            if (!match_rot && !match_refl)
                continue;

            const LinePermutation& rot_lp = rot_perms[r];
            if (match_rot) {
                ++stab_match_count;
                if (stab_perms.insert(rot_lp).second)
                    ++rot_fixes;
            }
            if (match_refl) {
                ++stab_match_count;
                LinePermutation refl_lp = rot_lp.compose(mirror_perm);
                if (stab_perms.insert(std::move(refl_lp)).second)
                    ++refl_fixes;
            }
        }
        // Same kernel argument as for G_P: rho^n fixes no O-matrix, so no two
        // matches collapse onto one line permutation in H_E either.
        kernel_collapse_free = kernel_collapse_free && (stab_match_count == stab_perms.size());

        ec.stabilizer_rotation_fixes = rot_fixes;
        ec.stabilizer_reflection_fixes = refl_fixes;

        std::vector<LinePermutation> stab_list(stab_perms.begin(), stab_perms.end());
        if (stab_list.empty())
            stab_list.push_back(LinePermutation::identity(n + 1));

        LinePermutationGroup stab_group = LinePermutationGroup::from_elements(stab_list);
        ec.stabilizer_order = stab_group.order();
        ec.stabilizer_image_order = stab_group.order();
        ec.stabilizer_kernel_order = 1;
        ec.e_sym_generators = stab_group.generators();
        ec.e_sym_elements = stab_group.elements();
        ec.e_sym_element_orders = stab_group.element_orders();
        ec.stabilizer_type = classify_euclidean_stabilizer(
            stab_group.order(), refl_fixes, stab_group.type());
        const RepresentativeCount reps = representative_count(
            n, options.no_mirror ? 2 * n : 4 * n, ec.stabilizer_order,
            options.no_mirror);
        ec.representatives = reps.value;
        ec.representatives_orbit = reps.orbit;
        ec.representatives_defined = reps.defined;
    }

    run_consistency_checks(result, pos, kernel_collapse_free, true);
    return result;
}

std::optional<SymmetryAnalysisResult> analyze_symmetries(
    const addline::OMatrix& matrix,
    const addline::ProjectiveOMatrix& form,
    size_t pos,
    const AnalyzerOptions& options) {
    const size_t n = static_cast<size_t>(matrix.n);
    if (n == 0 || form.n != matrix.n)
        return std::nullopt;

    addline::ClassIdentifiers identifiers =
        addline::class_identifiers(matrix, form);
    if (!identifiers.ok)
        return std::nullopt;

    auto key_less = [](const addline::ExactKey& a,
                       const addline::ExactKey& b) {
        return std::lexicographical_compare(
            a.bytes.begin(), a.bytes.end(), b.bytes.begin(), b.bytes.end());
    };
    auto key_string = [](const addline::ExactKey& key) {
        return std::string(reinterpret_cast<const char*>(key.bytes.data()),
                           key.bytes.size());
    };
    auto permutation_of = [](const std::vector<int>& relabel) {
        std::vector<size_t> values;
        values.reserve(relabel.size());
        for (int value : relabel) {
            if (value < 0)
                throw std::invalid_argument("negative line relabeling");
            values.push_back(static_cast<size_t>(value));
        }
        return LinePermutation(std::move(values));
    };
    auto fail = [&](const std::string& stage, const std::string& detail = {})
        -> std::optional<SymmetryAnalysisResult> {
        std::cerr << "[analyzer] generalized_failed pos=" << pos
                  << " stage=" << stage;
        if (!detail.empty()) std::cerr << " reason=\"" << detail << '"';
        std::cerr << '\n';
        return std::nullopt;
    };

    // First E-normalize the source chart, rebuild its projective closure, and
    // only then minimize over the projective cuts: at a multiple point the
    // closure of an unnormalized chart depends on its labels.
    addline::OMatrix normalized_source;
    addline::ExactKey normalized_source_d;
    bool have_normalized_source = false;
    std::string reason;
    std::vector<addline::ChartVariant> source_variants;
    if (!addline::chart_variants(form, static_cast<int>(n),
                                 source_variants, reason)) {
        return fail("source_charts", reason);
    }
    for (const addline::ChartVariant& variant : source_variants) {
        std::vector<addline::EuclideanImage> images;
        if (!addline::euclidean_images(variant.chart, true, images, reason))
            return fail("source_euclidean_orbit", reason);
        for (const addline::EuclideanImage& image : images) {
            if (!have_normalized_source ||
                key_less(image.d_key, normalized_source_d)) {
                normalized_source = image.image;
                normalized_source_d = image.d_key;
                have_normalized_source = true;
            }
        }
    }
    if (!have_normalized_source)
        return fail("source_normalization");

    const addline::ProjectiveOMatrix normalized_source_form =
        addline::projective_closure(normalized_source);

    // Pick the deterministic affine representative that realizes the P-key.
    addline::OMatrix canonical_matrix;
    addline::ExactKey canonical_d;
    bool have_canonical = false;
    for (size_t cut = 0; cut <= n; ++cut) {
        std::vector<addline::ChartVariant> variants;
        if (!addline::chart_variants(
                normalized_source_form, static_cast<int>(cut), variants, reason)) {
            reason.clear();
            continue;
        }
        for (const addline::ChartVariant& variant : variants) {
            std::vector<addline::EuclideanImage> images;
            if (!addline::euclidean_images(
                    variant.chart, true, images, reason))
                return fail("canonical_euclidean_orbit", reason);
            for (const addline::EuclideanImage& image : images) {
                if (!have_canonical || key_less(image.d_key, canonical_d)) {
                    canonical_matrix = image.image;
                    canonical_d = image.d_key;
                    have_canonical = true;
                }
            }
        }
    }
    if (!have_canonical)
        return fail("canonical_representative");
    if (canonical_d.bytes != identifiers.p_key.bytes)
        return fail("canonical_key_mismatch");

    // Keep the realizable order inside every point block.  The serializer
    // sorts a copy for comparison, but sorting each row of the actual matrix
    // independently can destroy the common local order and make a valid
    // starred arrangement impossible to replay.
    const addline::ProjectiveOMatrix canonical_form =
        addline::projective_closure(canonical_matrix);

    struct CutData {
        std::vector<addline::ChartVariant> variants;
        addline::ExactKey e_key;
        size_t e_representative = 0;
    };
    std::vector<CutData> cuts(n + 1);
    for (size_t cut = 0; cut <= n; ++cut) {
        if (!addline::chart_variants(canonical_form, static_cast<int>(cut),
                                     cuts[cut].variants, reason)) {
            return fail("canonical_chart_" + std::to_string(cut), reason);
        }
        bool have_key = false;
        for (size_t variant_index = 0;
             variant_index < cuts[cut].variants.size(); ++variant_index) {
            const addline::ChartVariant& variant =
                cuts[cut].variants[variant_index];
            addline::ExactKey candidate;
            if (!addline::exact_e_key(variant.chart, candidate, reason))
                return fail("chart_e_key_" + std::to_string(cut), reason);
            if (!have_key || key_less(candidate, cuts[cut].e_key)) {
                cuts[cut].e_key = std::move(candidate);
                cuts[cut].e_representative = variant_index;
                have_key = true;
            }
        }
        if (!have_key)
            return fail("chart_without_e_key_" + std::to_string(cut));
    }

    SymmetryAnalysisResult result;
    result.pos = pos;
    result.n = n;
    result.pid = identifiers.pid;

    size_t gp_match_count = 0;
    std::set<LinePermutation> p_perms;
    for (const CutData& cut : cuts) {
        for (const addline::ChartVariant& variant : cut.variants) {
            std::vector<addline::EuclideanImage> images;
            if (!addline::euclidean_images(
                    variant.chart, !options.no_mirror, images, reason)) {
                return fail("projective_symmetry_orbit", reason);
            }
            const LinePermutation chart_perm = permutation_of(variant.relabel);
            for (const addline::EuclideanImage& image : images) {
                if (image.d_key.bytes != canonical_d.bytes)
                    continue;
                const LinePermutation euclidean_perm =
                    permutation_of(image.relabel);
                const LinePermutation permutation =
                    chart_perm.compose(euclidean_perm);
                p_perms.insert(permutation);
                ++gp_match_count;
            }
        }
    }
    bool kernel_collapse_free = gp_match_count == p_perms.size();
    if (p_perms.empty())
        p_perms.insert(LinePermutation::identity(n + 1));

    const std::vector<LinePermutation> p_elements(
        p_perms.begin(), p_perms.end());
    const LinePermutationGroup p_group =
        LinePermutationGroup::from_elements(p_elements);
    result.p_sym_ord = p_group.order();
    result.p_sym_type = p_group.type();
    result.p_sym_generators = p_group.generators();
    result.p_sym_elements = p_group.elements();
    result.p_sym_element_orders = p_group.element_orders();

    std::vector<size_t> cut_to_e(n + 1, 0);
    std::vector<std::string> e_keys;
    e_keys.push_back(key_string(cuts[n].e_key));
    for (size_t cut = 0; cut < n; ++cut) {
        const std::string key = key_string(cuts[cut].e_key);
        auto found = std::find(e_keys.begin(), e_keys.end(), key);
        if (found == e_keys.end()) {
            e_keys.push_back(key);
            cut_to_e[cut] = e_keys.size() - 1;
        } else {
            cut_to_e[cut] = static_cast<size_t>(found - e_keys.begin());
        }
    }
    cut_to_e[n] = 0;
    result.cut_to_e_class = cut_to_e;

    const size_t num_classes = e_keys.size();
    result.e_classes.resize(num_classes);
    std::vector<size_t> representative_cut(num_classes, n);
    std::vector<bool> have_representative(num_classes, false);
    std::vector<size_t> cut_count(num_classes, 0);
    have_representative[0] = true;
    representative_cut[0] = n;
    for (size_t cut = 0; cut <= n; ++cut) {
        const size_t id = cut_to_e[cut];
        ++cut_count[id];
        if (!have_representative[id]) {
            have_representative[id] = true;
            representative_cut[id] = cut;
        }
    }

    for (size_t id = 0; id < num_classes; ++id) {
        ELocalClass& ec = result.e_classes[id];
        ec.local_e_id = id;
        ec.cut_count = cut_count[id];
        ec.eid = "E" + std::to_string(n) + "-" +
                 addline::base36(cuts[representative_cut[id]].e_key.hash);

        const addline::OMatrix& representative =
            cuts[representative_cut[id]].variants[
                cuts[representative_cut[id]].e_representative].chart;
        addline::ExactKey representative_d;
        if (!addline::exact_d_key(representative, representative_d, reason))
            return fail("e_representative_d_key", reason);

        std::vector<addline::EuclideanImage> images;
        if (!addline::euclidean_images(
                representative, !options.no_mirror, images, reason)) {
            return fail("e_stabilizer_orbit", reason);
        }

        size_t raw_matches = 0;
        size_t raw_rotation_fixes = 0;
        size_t raw_reflection_fixes = 0;
        std::set<LinePermutation> stabilizer_perms;
        for (const addline::EuclideanImage& image : images) {
            if (image.d_key.bytes != representative_d.bytes)
                continue;
            ++raw_matches;
            if (image.reflected)
                ++raw_reflection_fixes;
            else
                ++raw_rotation_fixes;
            stabilizer_perms.insert(permutation_of(image.relabel));
        }
        if (stabilizer_perms.empty())
            stabilizer_perms.insert(LinePermutation::identity(n + 1));
        kernel_collapse_free = kernel_collapse_free &&
                               raw_matches == stabilizer_perms.size();

        const std::vector<LinePermutation> stabilizer_elements(
            stabilizer_perms.begin(), stabilizer_perms.end());
        const LinePermutationGroup stabilizer_group =
            LinePermutationGroup::from_elements(stabilizer_elements);

        ec.stabilizer_order = raw_matches;
        ec.stabilizer_image_order = stabilizer_group.order();
        ec.stabilizer_kernel_order =
            ec.stabilizer_image_order == 0
                ? 0
                : ec.stabilizer_order / ec.stabilizer_image_order;
        ec.stabilizer_rotation_fixes = raw_rotation_fixes;
        ec.stabilizer_reflection_fixes = raw_reflection_fixes;
        ec.stabilizer_type = classify_euclidean_stabilizer(
            ec.stabilizer_order, raw_reflection_fixes,
            "C" + std::to_string(ec.stabilizer_order));
        ec.e_sym_generators = stabilizer_group.generators();
        ec.e_sym_elements = stabilizer_group.elements();
        ec.e_sym_element_orders = stabilizer_group.element_orders();
        const RepresentativeCount reps = representative_count(
            n, images.size(), ec.stabilizer_order, options.no_mirror);
        ec.representatives = reps.value;
        ec.representatives_orbit = reps.orbit;
        ec.representatives_defined = reps.defined;
    }

    run_consistency_checks(result, pos, kernel_collapse_free, false);
    return result;
}
