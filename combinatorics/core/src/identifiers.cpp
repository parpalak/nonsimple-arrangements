// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Roman Parpalak and Denis Utkin

#include "identifiers.hpp"

#include <algorithm>
#include <functional>
#include <numeric>
#include <set>
#include <utility>

namespace addline {

namespace {

using Point = std::vector<int>;
using Points = std::vector<Point>;

constexpr std::uint8_t kExtensionMarker = 0xff;
constexpr std::uint8_t kGroupedVersion = 0x01;
constexpr std::uint8_t kParallelVersion = 0x02;

bool key_less(const ExactKey& a, const ExactKey& b) {
    return std::lexicographical_compare(a.bytes.begin(), a.bytes.end(),
                                        b.bytes.begin(), b.bytes.end());
}

int mod(int value, int n) {
    value %= n;
    return value < 0 ? value + n : value;
}

std::vector<int> group_sizes(const OMatrix& o, int line) {
    if (!o.groups.empty() && !o.groups[line].empty()) return o.groups[line];
    return std::vector<int>(o.rows[line].size(), 1);
}

// Missing crossings form the parallel classes of an affine chart.
bool parallel_classes(const OMatrix& o, Points& classes, std::string& reason) {
    const int n = o.n;
    std::vector<int> parent(static_cast<std::size_t>(n));
    std::iota(parent.begin(), parent.end(), 0);
    std::function<int(int)> root = [&](int x) {
        if (parent[x] != x) parent[x] = root(parent[x]);
        return parent[x];
    };
    auto unite = [&](int a, int b) {
        a = root(a);
        b = root(b);
        if (a != b) parent[b] = a;
    };

    std::vector<std::vector<char>> meets(
        static_cast<std::size_t>(n), std::vector<char>(static_cast<std::size_t>(n), 0));
    for (int line = 0; line < n; ++line) {
        for (int other : o.rows[line]) {
            if (other < 0 || other >= n || other == line || meets[line][other]) {
                reason = "bad crossing order on line " + std::to_string(line);
                return false;
            }
            meets[line][other] = 1;
        }
    }
    for (int a = 0; a < n; ++a) {
        for (int b = a + 1; b < n; ++b) {
            if (meets[a][b] != meets[b][a]) {
                reason = "parallel relation is not symmetric";
                return false;
            }
            if (!meets[a][b]) unite(a, b);
        }
    }

    std::vector<Point> by_root(static_cast<std::size_t>(n));
    for (int line = 0; line < n; ++line) by_root[root(line)].push_back(line);
    for (Point& point : by_root) {
        if (point.size() <= 1) continue;
        for (int a : point) {
            for (int b : point) {
                if (a != b && meets[a][b]) {
                    reason = "parallelism is not transitive";
                    return false;
                }
            }
        }
        classes.push_back(std::move(point));
    }
    std::sort(classes.begin(), classes.end());
    return true;
}

bool finite_points(const OMatrix& o, Points& points, std::string& reason) {
    std::set<Point> unique;
    for (int line = 0; line < o.n; ++line) {
        const std::vector<int> sizes = group_sizes(o, line);
        std::size_t at = 0;
        for (int size : sizes) {
            if (size < 1 || at + static_cast<std::size_t>(size) > o.rows[line].size()) {
                reason = "bad point groups on line " + std::to_string(line);
                return false;
            }
            if (size > 1) {
                Point point{line};
                point.insert(point.end(), o.rows[line].begin() + at,
                             o.rows[line].begin() + at + size);
                std::sort(point.begin(), point.end());
                point.erase(std::unique(point.begin(), point.end()), point.end());
                if (static_cast<int>(point.size()) != size + 1) {
                    reason = "repeated line in a multiple point";
                    return false;
                }
                unique.insert(std::move(point));
            }
            at += static_cast<std::size_t>(size);
        }
        if (at != o.rows[line].size()) {
            reason = "point groups do not cover line " + std::to_string(line);
            return false;
        }
    }
    points.assign(unique.begin(), unique.end());
    return true;
}

void set_continuation(std::vector<std::uint8_t>& mask, int n, int line,
                      int right_position) {
    // RIGHT_POSITION is 1..n-2 and names the adjacency (right-1,right).
    const std::size_t q = static_cast<std::size_t>(line) * (n - 2) +
                          static_cast<std::size_t>(right_position - 1);
    mask[q / 8] |= static_cast<std::uint8_t>(1u << (q % 8));
}

bool serialize(const OMatrix& o, ExactKey& out, std::string& reason) {
    if (o.n < 2 || o.n > 255 || static_cast<int>(o.rows.size()) != o.n) {
        reason = "identifier requires 2 <= n <= 255 and n rows";
        return false;
    }

    Points parallels;
    if (!parallel_classes(o, parallels, reason)) return false;
    std::vector<int> parallel_class(static_cast<std::size_t>(o.n), -1);
    for (std::size_t id = 0; id < parallels.size(); ++id) {
        for (int line : parallels[id]) parallel_class[line] = static_cast<int>(id);
    }

    const std::size_t bit_count = static_cast<std::size_t>(o.n) * (o.n - 2);
    std::vector<std::uint8_t> mask((bit_count + 7) / 8, 0);
    std::vector<std::uint8_t> parallel_count(static_cast<std::size_t>(o.n), 0);
    std::vector<std::uint8_t> labels;
    labels.reserve(static_cast<std::size_t>(o.n) * (o.n - 1));
    bool has_multiple = false;

    for (int line = 0; line < o.n; ++line) {
        const std::vector<int> sizes = group_sizes(o, line);
        std::size_t at = 0;
        int position = 0;
        for (int size : sizes) {
            if (size < 1 || at + static_cast<std::size_t>(size) > o.rows[line].size()) {
                reason = "bad point groups on line " + std::to_string(line);
                return false;
            }
            std::vector<int> block(o.rows[line].begin() + at,
                                   o.rows[line].begin() + at + size);
            std::sort(block.begin(), block.end());
            for (int value : block) {
                if (value < 0 || value >= o.n || value == line) {
                    reason = "bad label on line " + std::to_string(line);
                    return false;
                }
                labels.push_back(static_cast<std::uint8_t>(value));
            }
            for (int j = 1; j < size; ++j) {
                set_continuation(mask, o.n, line, position + j);
            }
            if (size > 1) has_multiple = true;
            position += size;
            at += static_cast<std::size_t>(size);
        }
        if (at != o.rows[line].size()) {
            reason = "point groups do not cover line " + std::to_string(line);
            return false;
        }

        if (parallel_class[line] >= 0) {
            Point block;
            for (int other : parallels[parallel_class[line]]) {
                if (other != line) block.push_back(other);
            }
            std::sort(block.begin(), block.end());
            parallel_count[line] = static_cast<std::uint8_t>(block.size());
            for (int value : block) labels.push_back(static_cast<std::uint8_t>(value));
            for (int j = 1; j < static_cast<int>(block.size()); ++j) {
                set_continuation(mask, o.n, line, position + j);
            }
            position += static_cast<int>(block.size());
        }
        if (position != o.n - 1) {
            reason = "row does not account for every other line";
            return false;
        }
    }

    out.bytes = std::move(labels);
    if (parallels.empty()) {
        if (has_multiple) {
            out.bytes.push_back(kExtensionMarker);
            out.bytes.push_back(kGroupedVersion);
            out.bytes.insert(out.bytes.end(), mask.begin(), mask.end());
        }
    } else {
        out.bytes.push_back(kExtensionMarker);
        out.bytes.push_back(kParallelVersion);
        out.bytes.insert(out.bytes.end(), parallel_count.begin(), parallel_count.end());
        out.bytes.insert(out.bytes.end(), mask.begin(), mask.end());
    }
    out.hash = fnv1a64(out.bytes);
    return true;
}

// BASE is already resolved. A simple key is the row-major labels, so they are
// compared directly instead of building every orbit element; most candidates
// lose in the first few bytes.
bool update_rotated_simple_min(const OMatrix& base, int rotation,
                               ExactKey& best, bool have) {
    const std::size_t size =
        static_cast<std::size_t>(base.n) * (base.n - 1);
    rotation = mod(rotation, 2 * base.n);

    std::size_t at = 0;
    bool smaller = !have;
    auto consider = [&](int old_label) {
        const std::uint8_t label =
            static_cast<std::uint8_t>(mod(old_label - rotation, base.n));
        if (!smaller && label > best.bytes[at]) return false;
        if (!smaller && label < best.bytes[at]) smaller = true;
        ++at;
        return true;
    };
    for (int line = 0; line < base.n; ++line) {
        const int lifted = mod(line + rotation, 2 * base.n);
        const std::vector<int>& row = base.rows[lifted % base.n];
        if (lifted >= base.n) {
            for (auto it = row.rbegin(); it != row.rend(); ++it) {
                if (!consider(*it)) return false;
            }
        } else {
            for (int label : row) {
                if (!consider(label)) return false;
            }
        }
    }
    if (!smaller) return false;

    best.bytes.resize(size);
    at = 0;
    for (int line = 0; line < base.n; ++line) {
        const int lifted = mod(line + rotation, 2 * base.n);
        const std::vector<int>& row = base.rows[lifted % base.n];
        if (lifted >= base.n) {
            for (auto it = row.rbegin(); it != row.rend(); ++it)
                best.bytes[at++] =
                    static_cast<std::uint8_t>(mod(*it - rotation, base.n));
        } else {
            for (int label : row)
                best.bytes[at++] =
                    static_cast<std::uint8_t>(mod(label - rotation, base.n));
        }
    }
    best.hash = fnv1a64(best.bytes);
    return true;
}

void append_reversal(int pos, int width, Word& out, std::vector<int>& slots) {
    for (int d = 1; d < width; ++d) {
        for (int j = d - 1; j >= 0; --j) {
            out.push_back(Step{pos + j, 2});
            std::swap(slots[pos + j], slots[pos + j + 1]);
        }
    }
}

bool resolve(const OMatrix& o, OMatrix& simple, Points& points,
             Points& parallels, std::string& reason) {
    const WordResult grouped = word_from_omatrix(o);
    if (!grouped.ok) {
        reason = grouped.reason;
        return false;
    }
    if (!finite_points(o, points, reason) ||
        !parallel_classes(o, parallels, reason)) return false;

    std::vector<int> slots(static_cast<std::size_t>(o.n));
    std::iota(slots.begin(), slots.end(), 0);
    Word word;
    for (const Step& step : grouped.word) {
        if (step.pos < 0 || step.pos + step.k > o.n) {
            reason = "step outside the wiring diagram";
            return false;
        }
        append_reversal(step.pos, step.k, word, slots);
    }

    std::vector<std::pair<int, int>> blocks;
    for (const Point& point : parallels) {
        int first = o.n;
        int last = -1;
        for (int line : point) {
            const auto it = std::find(slots.begin(), slots.end(), line);
            if (it == slots.end()) {
                reason = "parallel line missing from final order";
                return false;
            }
            const int at = static_cast<int>(it - slots.begin());
            first = std::min(first, at);
            last = std::max(last, at);
        }
        if (last - first + 1 != static_cast<int>(point.size())) {
            reason = "parallel class is not consecutive";
            return false;
        }
        blocks.push_back({first, static_cast<int>(point.size())});
    }
    std::sort(blocks.begin(), blocks.end());
    for (const auto& block : blocks) append_reversal(block.first, block.second, word, slots);
    for (int i = 0; i < o.n; ++i) {
        if (slots[i] != o.n - 1 - i) {
            reason = "resolved word does not reach the reverse permutation";
            return false;
        }
    }

    const OMatrixResult built = omatrix_from_word(word, o.n);
    if (!built.ok) {
        reason = built.reason;
        return false;
    }
    simple = built.o;
    return true;
}

OMatrix mirror_simple(const OMatrix& o) {
    OMatrix out;
    out.n = o.n;
    out.rows.resize(static_cast<std::size_t>(o.n));
    out.groups.assign(static_cast<std::size_t>(o.n),
                      std::vector<int>(static_cast<std::size_t>(o.n - 1), 1));
    for (int i = 0; i < o.n; ++i) {
        const std::vector<int>& old = o.rows[o.n - 1 - i];
        for (int value : old) out.rows[i].push_back(o.n - 1 - value);
    }
    return out;
}

OMatrix rotate_simple(const OMatrix& o, int rotation) {
    OMatrix out;
    out.n = o.n;
    out.rows.resize(static_cast<std::size_t>(o.n));
    out.groups.assign(static_cast<std::size_t>(o.n),
                      std::vector<int>(static_cast<std::size_t>(o.n - 1), 1));
    rotation = mod(rotation, 2 * o.n);
    for (int i = 0; i < o.n; ++i) {
        const int lifted = mod(i + rotation, 2 * o.n);
        const int old_line = lifted % o.n;
        const bool reverse = lifted >= o.n;
        if (reverse) {
            for (auto it = o.rows[old_line].rbegin(); it != o.rows[old_line].rend(); ++it) {
                out.rows[i].push_back(mod(*it - rotation, o.n));
            }
        } else {
            for (int value : o.rows[old_line]) {
                out.rows[i].push_back(mod(value - rotation, o.n));
            }
        }
    }
    return out;
}

Points map_points(const Points& input, const std::function<int(int)>& map) {
    Points out = input;
    for (Point& point : out) {
        for (int& line : point) line = map(line);
        std::sort(point.begin(), point.end());
    }
    std::sort(out.begin(), out.end());
    return out;
}

bool collapse(const OMatrix& simple, const Points& points,
              const Points& parallels, OMatrix& out, std::string& reason) {
    const int n = simple.n;
    std::vector<int> parallel_of(static_cast<std::size_t>(n), -1);
    for (std::size_t id = 0; id < parallels.size(); ++id) {
        for (int line : parallels[id]) parallel_of[line] = static_cast<int>(id);
    }
    std::vector<int> point_of(static_cast<std::size_t>(n) * n, -1);
    for (std::size_t id = 0; id < points.size(); ++id) {
        for (int a : points[id]) {
            for (int b : points[id]) {
                if (a != b) point_of[static_cast<std::size_t>(a) * n + b] = static_cast<int>(id);
            }
        }
    }

    out.n = n;
    out.rows.assign(static_cast<std::size_t>(n), {});
    out.groups.assign(static_cast<std::size_t>(n), {});
    for (int line = 0; line < n; ++line) {
        const std::vector<int>& row = simple.rows[line];
        for (std::size_t at = 0; at < row.size();) {
            const int other = row[at];
            if (parallel_of[line] >= 0 && parallel_of[line] == parallel_of[other]) {
                ++at;
                continue;
            }
            const int point = point_of[static_cast<std::size_t>(line) * n + other];
            if (point < 0) {
                out.rows[line].push_back(other);
                out.groups[line].push_back(1);
                ++at;
                continue;
            }
            const int width = static_cast<int>(points[point].size()) - 1;
            if (at + static_cast<std::size_t>(width) > row.size()) {
                reason = "resolved multiple point runs off a row";
                return false;
            }
            for (int j = 0; j < width; ++j) {
                const int member = row[at + j];
                if (point_of[static_cast<std::size_t>(line) * n + member] != point) {
                    reason = "resolved multiple point is not consecutive";
                    return false;
                }
                out.rows[line].push_back(member);
            }
            out.groups[line].push_back(width);
            at += static_cast<std::size_t>(width);
        }
    }
    return true;
}

// resolve() writes a parallel class as a block of crossings at the end of the
// word. A rotation moves it to the front once the sweep has passed the point
// at infinity of the class.
enum class BlockSide { Back, Front, Split };

BlockSide block_side(const OMatrix& simple, const Point& members) {
    const std::size_t k = members.size();
    auto is_member = [&](int line) {
        return std::find(members.begin(), members.end(), line) != members.end();
    };
    bool all_front = true;
    bool all_back = true;
    for (int line : members) {
        const std::vector<int>& row = simple.rows[line];
        if (row.size() < k - 1) return BlockSide::Split;
        std::size_t front = 0;
        std::size_t back = 0;
        for (std::size_t i = 0; i + 1 < k; ++i) {
            if (is_member(row[i])) ++front;
            if (is_member(row[row.size() - 1 - i])) ++back;
        }
        if (front + 1 != k) all_front = false;
        if (back + 1 != k) all_back = false;
    }
    // A class that fills a whole row (every line parallel to every other) is
    // both, and then nothing has to move.
    if (all_back) return BlockSide::Back;
    if (all_front) return BlockSide::Front;
    return BlockSide::Split;
}

// Deleting a front block leaves its lines, consecutive at the left edge, in
// reversed order: the deleted crossings were the swap. Undo it.
bool undo_front_block(const Point& members, std::vector<int>& label_map,
                      std::string& reason) {
    for (std::size_t i = 1; i < members.size(); ++i) {
        if (members[i] != members[0] + static_cast<int>(i)) {
            reason = "a parallel class crossing first does not occupy "
                     "consecutive labels";
            return false;
        }
    }
    const int first = members.front();
    const int last = members.back();
    for (int line = first; line <= last; ++line) {
        label_map[line] = first + last - line;
    }
    return true;
}

OMatrix relabelled(const OMatrix& o, const std::vector<int>& label_map) {
    OMatrix out;
    out.n = o.n;
    out.rows.assign(static_cast<std::size_t>(o.n), {});
    out.groups.assign(static_cast<std::size_t>(o.n), {});
    for (int line = 0; line < o.n; ++line) {
        const std::size_t target = static_cast<std::size_t>(label_map[line]);
        out.rows[target].reserve(o.rows[line].size());
        for (int value : o.rows[line]) out.rows[target].push_back(label_map[value]);
        out.groups[target] = group_sizes(o, line);
    }
    return out;
}

// A rotation that splits a parallel class between the two ends shows the
// perturbation of resolve(), not a chart of the arrangement; ALIGNED is then
// false. 2m of the 2n rotations survive for m directions.
bool euclidean_image(const OMatrix& base, const Points& base_points,
                     const Points& base_parallels, int rotation,
                     OMatrix& out, std::vector<int>& label_map, bool& aligned,
                     std::string& reason) {
    const int n = base.n;
    const OMatrix moved = rotate_simple(base, rotation);
    auto shift = [n, rotation](int x) { return mod(x - rotation, n); };
    const Points moved_points = map_points(base_points, shift);
    const Points moved_parallels = map_points(base_parallels, shift);

    label_map.resize(static_cast<std::size_t>(n));
    std::iota(label_map.begin(), label_map.end(), 0);
    aligned = true;
    bool corrected = false;
    for (const Point& members : moved_parallels) {
        const BlockSide side = block_side(moved, members);
        if (side == BlockSide::Split) {
            aligned = false;
            return true;
        }
        if (side == BlockSide::Front) {
            if (!undo_front_block(members, label_map, reason)) return false;
            corrected = true;
        }
    }

    OMatrix grouped;
    if (!collapse(moved, moved_points, moved_parallels, grouped, reason))
        return false;
    out = corrected ? relabelled(grouped, label_map) : std::move(grouped);
    return true;
}

bool e_key_from_resolved(const OMatrix& simple, const Points& points,
                         const Points& parallels, ExactKey& out,
                         OMatrix* representative,
                         std::string& reason) {
    bool have = false;

    // The hot path: the general one below only adds bookkeeping that is a
    // no-op for a simple arrangement.
    if (points.empty() && parallels.empty()) {
        const std::size_t key_size =
            static_cast<std::size_t>(simple.n) * (simple.n - 1);
        if (simple.rows.size() != static_cast<std::size_t>(simple.n) ||
            (have && out.bytes.size() != key_size)) {
            reason = "invalid simple Euclidean image";
            return false;
        }
        for (const std::vector<int>& row : simple.rows) {
            if (row.size() != static_cast<std::size_t>(simple.n - 1) ||
                std::any_of(row.begin(), row.end(), [&](int label) {
                    return label < 0 || label >= simple.n;
                })) {
                reason = "invalid simple Euclidean image";
                return false;
            }
        }
        for (int reflected = 0; reflected < 2; ++reflected) {
            const OMatrix base = reflected ? mirror_simple(simple) : simple;
            for (int rotation = 0; rotation < 2 * simple.n; ++rotation) {
                const bool better =
                    update_rotated_simple_min(base, rotation, out, have);
                if (better) {
                    if (representative)
                        *representative = rotate_simple(base, rotation);
                    have = true;
                }
            }
        }
        return have;
    }

    for (int reflected = 0; reflected < 2; ++reflected) {
        OMatrix base = reflected ? mirror_simple(simple) : simple;
        Points base_points = reflected
            ? map_points(points, [n = simple.n](int x) { return n - 1 - x; })
            : points;
        Points base_parallels = reflected
            ? map_points(parallels, [n = simple.n](int x) { return n - 1 - x; })
            : parallels;

        for (int rotation = 0; rotation < 2 * simple.n; ++rotation) {
            OMatrix grouped;
            std::vector<int> label_map;
            bool aligned = false;
            if (!euclidean_image(base, base_points, base_parallels, rotation,
                                 grouped, label_map, aligned, reason))
                return false;
            if (!aligned) continue;
            ExactKey candidate;
            if (!serialize(grouped, candidate, reason)) return false;
            if (!have || key_less(candidate, out)) {
                out = std::move(candidate);
                if (representative) *representative = std::move(grouped);
                have = true;
            }
        }
    }
    if (!have) reason = "empty Euclidean orbit";
    return have;
}

std::vector<std::vector<int>> multiple_runs(const ProjectiveOMatrix& p, int line) {
    std::vector<std::vector<int>> runs;
    std::vector<char> done(p.rows[line].size(), 0);
    for (std::size_t i = 0; i < p.rows[line].size(); ++i) {
        if (done[i]) continue;
        std::vector<int> positions;
        for (std::size_t j = i; j < p.rows[line].size(); ++j) {
            if (p.point[line][j] == p.point[line][i]) {
                positions.push_back(static_cast<int>(j));
                done[j] = 1;
            }
        }
        if (positions.size() > 1) runs.push_back(std::move(positions));
    }
    return runs;
}

bool projective_form_ok(const ProjectiveOMatrix& p, std::string& reason) {
    if (p.n < 2 || p.n > 255 ||
        static_cast<int>(p.rows.size()) != p.n + 1 ||
        p.point.size() != p.rows.size()) {
        reason = "bad projective row count";
        return false;
    }
    for (int line = 0; line <= p.n; ++line) {
        if (static_cast<int>(p.rows[line].size()) != p.n ||
            p.point[line].size() != p.rows[line].size()) {
            reason = "bad projective row on line " + std::to_string(line);
            return false;
        }
    }
    return true;
}

}  // namespace

std::uint64_t fnv1a64(const std::vector<std::uint8_t>& bytes) {
    std::uint64_t h = 0xcbf29ce484222325ULL;
    for (std::uint8_t byte : bytes) {
        h ^= static_cast<std::uint64_t>(byte);
        h *= 0x100000001b3ULL;
    }
    return h;
}

std::string base36(std::uint64_t value) {
    static const char digits[] = "0123456789abcdefghijklmnopqrstuvwxyz";
    if (value == 0) return "0";
    std::string out;
    while (value) {
        out.push_back(digits[value % 36]);
        value /= 36;
    }
    std::reverse(out.begin(), out.end());
    return out;
}

bool chart_variants(const ProjectiveOMatrix& form, int infinity,
                    std::vector<ChartVariant>& out, std::string& reason) {
    out.clear();
    if (!projective_form_ok(form, reason)) return false;
    if (infinity < 0 || infinity > form.n) {
        reason = "line at infinity is out of range";
        return false;
    }

    const std::vector<std::vector<int>> runs =
        multiple_runs(form, infinity);
    ProjectiveOMatrix candidate = form;
    const std::vector<int> original = form.rows[infinity];
    std::string last_rejection;

    std::function<void(std::size_t)> visit = [&](std::size_t run_index) {
        if (run_index == runs.size()) {
            const RotationResult chart = chart_of(candidate, infinity);
            if (!chart.ok) {
                last_rejection = chart.reason;
                return;
            }
            const WordResult word = word_from_omatrix(chart.o);
            if (!word.ok) {
                last_rejection = word.reason;
                return;
            }
            out.push_back(ChartVariant{chart.o, chart.relabel});
            return;
        }

        const std::vector<int>& positions = runs[run_index];
        std::vector<int> perm(positions.size());
        std::iota(perm.begin(), perm.end(), 0);
        do {
            for (std::size_t i = 0; i < positions.size(); ++i) {
                candidate.rows[infinity][positions[i]] =
                    original[positions[perm[i]]];
            }
            visit(run_index + 1);
        } while (std::next_permutation(perm.begin(), perm.end()));
    };
    visit(0);

    if (out.empty()) {
        reason = "no realizable ordering for the selected affine chart";
        if (!last_rejection.empty()) reason += ": " + last_rejection;
        return false;
    }
    return true;
}

bool euclidean_images(const OMatrix& chart, bool include_reflections,
                      std::vector<EuclideanImage>& out,
                      std::string& reason) {
    out.clear();
    OMatrix simple;
    Points points, parallels;
    if (!resolve(chart, simple, points, parallels, reason)) return false;

    const int reflection_count = include_reflections ? 2 : 1;
    for (int reflected = 0; reflected < reflection_count; ++reflected) {
        const OMatrix base = reflected ? mirror_simple(simple) : simple;
        const Points base_points = reflected
            ? map_points(points, [n = simple.n](int x) { return n - 1 - x; })
            : points;
        const Points base_parallels = reflected
            ? map_points(parallels, [n = simple.n](int x) { return n - 1 - x; })
            : parallels;

        for (int rotation = 0; rotation < 2 * simple.n; ++rotation) {
            EuclideanImage image;
            std::vector<int> label_map;
            bool aligned = false;
            if (!euclidean_image(base, base_points, base_parallels, rotation,
                                 image.image, label_map, aligned, reason))
                return false;
            if (!aligned) continue;
            if (!serialize(image.image, image.d_key, reason)) return false;
            image.rotation = rotation;
            image.reflected = reflected != 0;
            image.relabel.resize(static_cast<std::size_t>(simple.n) + 1);
            for (int line = 0; line < simple.n; ++line) {
                const int base_line = reflected ? simple.n - 1 - line : line;
                image.relabel[line] = label_map[mod(base_line - rotation, simple.n)];
            }
            image.relabel[simple.n] = simple.n;
            out.push_back(std::move(image));
        }
    }
    return true;
}

bool exact_d_key(const OMatrix& o, ExactKey& out, std::string& reason) {
    const WordResult valid = word_from_omatrix(o);
    if (!valid.ok) {
        reason = valid.reason;
        return false;
    }
    return serialize(o, out, reason);
}

bool exact_e_key(const OMatrix& o, ExactKey& out, std::string& reason) {
    OMatrix simple;
    Points points, parallels;
    if (!resolve(o, simple, points, parallels, reason)) return false;
    return e_key_from_resolved(simple, points, parallels, out, nullptr, reason);
}

namespace {

// projective_closure() orients its rows by the labels of its chart, so two
// E-equivalent starred words can expose different cuts. Closing the
// E-normalized chart makes the cuts a property of the arrangement.
bool normalize_projective(const ProjectiveOMatrix& p, ProjectiveOMatrix& out,
                          std::string& reason) {
    std::vector<ChartVariant> source_variants;
    if (!chart_variants(p, p.n, source_variants, reason)) return false;
    bool have = false;
    ExactKey best;
    OMatrix representative;
    for (const ChartVariant& variant : source_variants) {
        OMatrix simple;
        Points points, parallels;
        if (!resolve(variant.chart, simple, points, parallels, reason))
            return false;
        ExactKey candidate;
        OMatrix image;
        if (!e_key_from_resolved(simple, points, parallels, candidate, &image,
                                 reason)) return false;
        if (!have || key_less(candidate, best)) {
            best = std::move(candidate);
            representative = std::move(image);
            have = true;
        }
    }
    if (!have) {
        reason = "no serializable normalization of the source chart";
        return false;
    }
    out = projective_closure(representative);
    return true;
}

}  // namespace

bool exact_p_key(const ProjectiveOMatrix& p, ExactKey& out,
                 std::string& reason) {
    if (!projective_form_ok(p, reason)) return false;
    ProjectiveOMatrix normalized;
    if (!normalize_projective(p, normalized, reason)) return false;

    bool have = false;
    for (int infinity = 0; infinity <= normalized.n; ++infinity) {
        std::vector<ChartVariant> variants;
        std::string local_reason;
        if (!chart_variants(normalized, infinity, variants, local_reason))
            continue;
        for (const ChartVariant& variant : variants) {
            ExactKey candidate;
            if (!exact_e_key(variant.chart, candidate, reason)) return false;
            if (!have || key_less(candidate, out)) {
                out = std::move(candidate);
                have = true;
            }
        }
    }

    if (!have)
        reason = "no affine chart of the projective arrangement is serializable";
    return have;
}

ClassIdentifiers class_identifiers(const OMatrix& chart,
                                   const ProjectiveOMatrix& form) {
    ClassIdentifiers out;
    if (!exact_d_key(chart, out.d_key, out.reason) ||
        !exact_e_key(chart, out.e_key, out.reason) ||
        !exact_p_key(form, out.p_key, out.reason)) return out;
    out.did = "D" + std::to_string(chart.n) + "-" + base36(out.d_key.hash);
    out.eid = "E" + std::to_string(chart.n) + "-" + base36(out.e_key.hash);
    out.pid = "P" + std::to_string(chart.n) + "-" + base36(out.p_key.hash);
    out.ok = true;
    return out;
}

}  // namespace addline
