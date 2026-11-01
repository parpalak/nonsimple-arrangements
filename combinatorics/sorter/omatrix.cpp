// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Roman Parpalak and Denis Utkin

#include "omatrix.hpp"

#include <algorithm>
#include <numeric>
#include <stdexcept>

size_t OMatrix::n() const {
    return intersections.size();
}

size_t OMatrix::norm(size_t l) const {
    return (l + n()*2) % n();
}

size_t OMatrix::norm2(size_t l) const {
    return (l + n()*2) % (2*n());
}

size_t OMatrix::get_parallels_ind(size_t l) const {
    auto it = parallels.find(l);
    if (it == parallels.end())
        return l;

    return it->second;
}

size_t OMatrix::get_val(size_t rotation, size_t i, size_t j) const {
    size_t v;
    size_t l = norm2(i + rotation);
    if (l < n()) {
        v = intersections[l][j];
    }
    else {
        l = norm(l);
        l = get_parallels_ind(l);
        auto &line = intersections[l];
        v = line[line.size() - 1 - j];
    }

    if (rotation < n() && v >= rotation || rotation >= n() && v < norm(rotation)) {
        return norm(v - rotation);
    }

    v = get_parallels_ind(v);
    return norm(v - rotation);
}

bool OMatrix::can_rotate(size_t rotation) const {
    if (rotation >= 2*n())
        return false;

    size_t rotated = norm(rotation);
    auto p = parallels.find(rotated);
    if (p == parallels.end())
        return true;

    return p->second >= rotated;
}

size_t OMatrix::get_line_len(size_t rotation, size_t i) const {
    size_t l = i + rotation;
    if (l < n()) {
        return intersections[l].size();
    }
    else {
        l = norm(l);
        l = get_parallels_ind(l);
        return intersections[l].size();
    }
}

size_t OMatrix::rotate_line_num(size_t rotation, size_t l) const {
    if (l >= rotation)
        return l - rotation;

    return norm(l - rotation);
}

std::optional<OMatrix> OMatrix::rotate(size_t rotation) const {
    rotation %= (2*n());
    if (rotation == 0)
        return *this;

    if (!can_rotate(rotation))
        return {};

    std::vector<std::vector<size_t>> new_intersections;
    for (int i = 0; i < n(); ++i)
    {
        std::vector<size_t> line;
        size_t len = get_line_len(rotation, i);
        for (size_t j = 0; j < len; ++j)
        {
            line.push_back(get_val(rotation, i, j));
        }
        new_intersections.push_back(line);
    }

    std::map<size_t, size_t> new_parallels;
    for (auto &p : parallels) {
        new_parallels[rotate_line_num(rotation, p.first)] = rotate_line_num(rotation, p.second);
    }

    return OMatrix{new_intersections, new_parallels};
}

size_t OMatrix::get_num_vertices() const {
    size_t result = 0;
    for (auto &l : intersections)
        result += l.size();

    return result / 2;
}

std::vector<size_t> OMatrix::get_generators() const {
    std::vector<size_t> lines;
    std::vector<size_t> pos;
    for (int i = 0; i < n(); ++i)
    {
        lines.push_back(i);
        pos.push_back(0);
    }

    std::vector<size_t> generators;

    size_t num_vertices = get_num_vertices();
    for (int i = 0; i < num_vertices; ++i)
    {
        bool found = false;
        for (size_t gen = 0; gen < n() - 1; ++gen)
        {
            size_t left = lines[gen];
            size_t right = lines[gen + 1];
            if (pos[left] < intersections[left].size() && pos[right] < intersections[right].size()
                && intersections[left][pos[left]] == right && intersections[right][pos[right]] == left)
            {
                pos[left]++;
                pos[right]++;
                lines[gen] = right;
                lines[gen + 1] = left;
                generators.push_back(gen);
                found = true;
                break;
            }
        }

        if (!found)
            throw std::runtime_error("Invalid OMatrix");
    }

    return generators;
}

OMatrix OMatrix::mirror() const {
    std::vector<std::vector<size_t>> new_intersections;
    for (size_t i = n(); i --> 0;)
    {
        std::vector<size_t> line;
        for (auto &l : intersections[i])
        {
            line.push_back(n() - 1 - l);
        }

        new_intersections.push_back(line);
    }

    std::map<size_t, size_t> new_parallels;
    for (auto &p : parallels) {
        new_parallels[n() - 1 - p.first] = n() - 1 - p.second;
    }

    return OMatrix{new_intersections, new_parallels};
}

std::optional<OMatrix> OMatrix::projective_rotate(size_t val) const {
    if (val >= n())
        return {};

    std::vector<std::vector<size_t>> temp;
    temp.reserve(n() + 1);
    for (size_t i = 0; i < n(); ++i) {
        std::vector<size_t> line = intersections[i];
        line.push_back(n());
        temp.push_back(std::move(line));
    }

    std::vector<size_t> inf_line;
    inf_line.reserve(n());
    for (size_t i = 0; i < n(); ++i) {
        inf_line.push_back(i);
    }
    temp.push_back(std::move(inf_line));

    if (temp[val].size() != n())
        return {};

    std::vector<size_t> reord(n() + 1, n() + 1);
    for (size_t i = 0; i < n(); ++i) {
        size_t old_line = temp[val][i];
        if (old_line > n())
            return {};
        reord[old_line] = i;
    }
    reord[n()] = n() - 1;

    std::vector<std::vector<size_t>> new_intersections;
    new_intersections.reserve(n());
    for (size_t i = 0; i < n(); ++i) {
        std::vector<size_t> new_line;
        size_t v = temp[val][i];
        if (v > n())
            return {};

        auto &temp_line = temp[v];
        if (temp_line.size() != n())
            return {};

        std::ptrdiff_t val_pos = -1;
        for (size_t j = 0; j < n(); ++j) {
            if (temp_line[j] == val) {
                val_pos = static_cast<std::ptrdiff_t>(j);
                break;
            }
        }
        if (val_pos < 0)
            return {};

        if (v < val) {
            for (std::ptrdiff_t j = val_pos - 1; j >= 0; --j) {
                size_t mapped = reord[temp_line[static_cast<size_t>(j)]];
                if (mapped > n())
                    return {};
                new_line.push_back(mapped);
            }

            for (std::ptrdiff_t j = static_cast<std::ptrdiff_t>(n()) - 1; j > val_pos; --j) {
                size_t mapped = reord[temp_line[static_cast<size_t>(j)]];
                if (mapped > n())
                    return {};
                new_line.push_back(mapped);
            }
        }
        else {
            for (size_t j = static_cast<size_t>(val_pos) + 1; j < n(); ++j) {
                size_t mapped = reord[temp_line[j]];
                if (mapped > n())
                    return {};
                new_line.push_back(mapped);
            }

            for (size_t j = 0; j < static_cast<size_t>(val_pos); ++j) {
                size_t mapped = reord[temp_line[j]];
                if (mapped > n())
                    return {};
                new_line.push_back(mapped);
            }
        }

        new_intersections.push_back(std::move(new_line));
    }

    return OMatrix{new_intersections, {}};
}

uint64_t OMatrix::hash() const {
    if (cached_hash.has_value())
        return *cached_hash;

    uint64_t h = 0xcbf29ce484222325ULL;
    const uint64_t p = 0x100000001b3ULL;

    for (auto &line : intersections) {
        for (auto v : line) {
            h ^= (uint64_t) v;
            h *= p;
        }
    }

    cached_hash = h;
    return h;
}

OMatrix OMatrix::min_o() const {
    // Canonical representative = lexicographically minimal O-matrix across the
    // 2n Euclidean rotations and their mirror. For closed matrices every line
    // has n-1 intersections, so comparing intersections lexicographically
    // matches the byte-buffer order used by the exact-cache path
    // (compact_key_less). The identifier hash is then hash() of this rep.
    OMatrix result = *this;

    for (size_t i = 1; i < 2 * n(); ++i)
    {
        auto r = rotate(i);
        if (r.has_value() && r->intersections < result.intersections)
            result = std::move(*r);
    }

    OMatrix mirrored = mirror();
    for (size_t i = 0; i < 2 * n(); ++i)
    {
        auto r = mirrored.rotate(i);
        if (r.has_value() && r->intersections < result.intersections)
            result = std::move(*r);
    }

    return result;
}

std::optional<OMatrix> OMatrix::min_projective_o() const {
    OMatrix best = min_o();

    for (size_t i = 0; i < n(); ++i) {
        auto proj = projective_rotate(i);
        if (!proj.has_value())
            continue;

        OMatrix candidate = proj->min_o();
        if (candidate.intersections < best.intersections)
            best = std::move(candidate);
    }

    return best;
}

bool OMatrix::operator==(const OMatrix& other) const {
    return intersections == other.intersections;
}

std::optional<OMatrix> get_OMatrix(std::vector<size_t> gens) {
    if (gens.empty())
        return {};

    size_t n = *std::max_element(gens.begin(), gens.end()) + 2;
    std::vector<size_t> lines(n);
    std::iota(lines.begin(), lines.end(), 0); // fill 0 1 2 ...
    std::vector<std::vector<size_t>> intersections(n);

    for (size_t gen : gens)
    {
        size_t first = lines[gen];
        size_t second = lines[gen + 1];
        if (first > second)
            return {};

        intersections[first].push_back(second);
        intersections[second].push_back(first);

        lines[gen] = second;
        lines[gen + 1] = first;
    }

    std::map<size_t, size_t> parallels;

    for (int i = 0; i < n - 1; ++i)
    {
        if (lines[i] < lines[i + 1])
        {
            if (parallels.count(lines[i]) || parallels.count(lines[i + 1]))
                return {};

            parallels[lines[i]] = lines[i + 1];
            parallels[lines[i + 1]] = lines[i];
        }
    }

    return OMatrix{intersections, parallels};
}
