// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Roman Parpalak and Denis Utkin

#include "wire_print.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <optional>
#include <ostream>

namespace {

struct Bits {
    std::vector<std::uint8_t> b;

    explicit Bits(std::size_t n = 0) : b(n, 0) {}

    void set(std::size_t i) {
        if (i < b.size()) b[i] = 1;
    }

    bool get(std::size_t i) const {
        return (i < b.size()) ? (b[i] != 0) : false;
    }

    std::size_t size() const { return b.size(); }

    void or_with(const Bits& oth) {
        const std::size_t n = std::min(b.size(), oth.b.size());
        for (std::size_t i = 0; i < n; ++i)
            b[i] = static_cast<std::uint8_t>(b[i] | oth.b[i]);
    }
};

bool is_compatible(const Bits& s1, const Bits& s2) {
    for (std::size_t i = 0; i < s1.size(); ++i) {
        if (!s1.get(i))
            continue;
        if (i > 0 && s2.get(i - 1))
            return false;
        if (s2.get(i + 1))
            return false;
    }
    return true;
}

void append_vertical_lines(std::ostream& os, std::size_t count) {
    for (std::size_t i = 0; i < count; ++i) {
        if (i > 0) os << "  ";
        os << '|';
    }
}

}  // namespace

void print_OMatrix(OMatrix& o) {
    std::cout << std::endl;

    int max_num_len = static_cast<int>(std::ceil(std::log10(o.n())));
    for (std::size_t i = 0; i < o.n(); ++i) {
        std::cout << std::setw(max_num_len) << i << " || " << std::setw(max_num_len);
        if (o.parallels.count(i))
            std::cout << o.parallels[i];
        else
            std::cout << i;

        std::cout << ") ";

        auto& intersections = o.intersections[i];
        for (std::size_t j = 0; j < intersections.size(); ++j) {
            std::cout << std::setw(max_num_len) << intersections[j];
            if (j + 1 < intersections.size())
                std::cout << " ";
        }
        std::cout << std::endl;
    }
    std::cout << std::endl;
}

void print_wire(std::vector<std::size_t> gens) {
    std::size_t n = 0;
    if (!gens.empty()) {
        n = *std::max_element(gens.begin(), gens.end()) + 2;
    }

    append_vertical_lines(std::cout, n);
    std::cout << '\n';

    for (std::size_t g : gens) {
        append_vertical_lines(std::cout, g);
        if (g > 0) {
            std::cout << "  ";
        }
        std::cout << " >< ";
        if (n - g - 2 > 0)
            std::cout << "  ";

        append_vertical_lines(std::cout, n - g - 2);
        std::cout << '\n';
    }

    append_vertical_lines(std::cout, n);
}

void print_wire_condensed(std::vector<std::size_t> gens) {
    std::size_t n = 0;
    if (!gens.empty()) {
        n = *std::max_element(gens.begin(), gens.end()) + 2;
    }

    std::vector<std::optional<Bits>> rows;
    rows.reserve(gens.size());

    for (std::size_t g : gens) {
        Bits bs(n > 0 ? (n - 1) : 0);
        bs.set(g);
        rows.emplace_back(std::move(bs));
    }

    for (std::size_t i = 0; i < rows.size(); ++i) {
        if (!rows[i].has_value())
            continue;

        std::size_t last_j = i;
        for (std::size_t j = i; j > 0; ) {
            --j;
            if (!rows[j].has_value())
                continue;

            if (!is_compatible(*rows[i], *rows[j]))
                break;
            last_j = j;
        }

        if (last_j != i) {
            rows[last_j]->or_with(*rows[i]);
            rows[i].reset();
        }
    }

    append_vertical_lines(std::cout, n);
    std::cout << '\n';

    std::vector<std::size_t> lines(n);
    std::iota(lines.begin(), lines.end(), 0);

    for (auto& opt : rows) {
        if (!opt.has_value()) continue;
        const Bits& bs = *opt;

        for (std::size_t i = 0; i < n * 2 - 1; ++i) {
            if (i % 2 == 0) {
                std::size_t l = i / 2;
                bool cross = ((l > 0) && bs.get(l - 1)) || bs.get(l);
                std::cout << (cross ? ' ' : '|');
            } else {
                std::size_t l = i / 2;
                if (bs.get(l)) {
                    std::cout << "><";
                    std::swap(lines[l], lines[l + 1]);
                } else {
                    std::cout << "  ";
                }
            }
        }
        std::cout << '\n';
    }

    append_vertical_lines(std::cout, n);
}
