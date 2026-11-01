// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Roman Parpalak and Denis Utkin

#include "state_io.hpp"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <vector>

namespace {

const char STATE_MAGIC[16] = {
    'S', 'O', 'R', 'T', 'E', 'R', 'S', 'T',
    'A', 'T', 'E', '0', '0', '0', '0', '1'
};

constexpr std::uint64_t STATE_VERSION = 1;

void write_u64(std::ostream& out, std::uint64_t value) {
    out.write(reinterpret_cast<const char*>(&value), sizeof(value));
    if (!out)
        throw std::runtime_error("failed to write state file");
}

std::uint64_t read_u64(std::istream& in) {
    std::uint64_t value = 0;
    in.read(reinterpret_cast<char*>(&value), sizeof(value));
    if (!in)
        throw std::runtime_error("failed to read state file");
    return value;
}

void write_exact_arena(std::ostream& out, const FixedLenExactKeyArena& arena) {
    write_u64(out, static_cast<std::uint64_t>(arena.n));
    write_u64(out, static_cast<std::uint64_t>(arena.key_len));
    write_u64(out, static_cast<std::uint64_t>(arena.bytes.size()));
    if (!arena.bytes.empty()) {
        out.write(reinterpret_cast<const char*>(arena.bytes.data()),
                  static_cast<std::streamsize>(arena.bytes.size()));
        if (!out)
            throw std::runtime_error("failed to write exact key arena");
    }
}

void read_exact_arena(std::istream& in, FixedLenExactKeyArena& arena, std::size_t expected_n) {
    std::uint64_t n = read_u64(in);
    std::uint64_t key_len = read_u64(in);
    std::uint64_t byte_size = read_u64(in);

    if (n != expected_n)
        throw std::runtime_error("state arena n mismatch");
    if (n == 0 || n > 255)
        throw std::runtime_error("state arena has invalid n");
    if (key_len != n * (n - 1))
        throw std::runtime_error("state arena key_len mismatch");
    if (byte_size % key_len != 0)
        throw std::runtime_error("state arena byte size is not a whole number of keys");
    if ((byte_size / key_len) > std::numeric_limits<std::uint32_t>::max())
        throw std::runtime_error("state arena has too many keys for uint32 ids");

    arena.n = static_cast<std::size_t>(n);
    arena.key_len = static_cast<std::size_t>(key_len);
    arena.bytes.resize(static_cast<std::size_t>(byte_size));
    if (!arena.bytes.empty()) {
        in.read(reinterpret_cast<char*>(arena.bytes.data()),
                static_cast<std::streamsize>(arena.bytes.size()));
        if (!in)
            throw std::runtime_error("failed to read exact key arena");
    }
    arena.rebuild_index();
}

void write_hit_bits(std::ostream& out, const FixedLenExactKeyArena& arena) {
    std::size_t count = arena.size();
    std::size_t byte_count = (count + 7) / 8;
    write_u64(out, static_cast<std::uint64_t>(count));
    std::vector<std::uint8_t> packed(byte_count, 0);
    for (std::size_t i = 0; i < count && i < arena.hit.size(); ++i) {
        if (arena.hit[i])
            packed[i / 8] |= static_cast<std::uint8_t>(1 << (i % 8));
    }
    if (byte_count > 0) {
        out.write(reinterpret_cast<const char*>(packed.data()),
                  static_cast<std::streamsize>(byte_count));
        if (!out)
            throw std::runtime_error("failed to write hit bits");
    }
}

void read_hit_bits(std::istream& in, FixedLenExactKeyArena& arena) {
    std::uint64_t count = read_u64(in);
    if (count != static_cast<std::uint64_t>(arena.size()))
        throw std::runtime_error("hit bits count mismatch");

    std::size_t byte_count = (static_cast<std::size_t>(count) + 7) / 8;
    std::vector<std::uint8_t> packed(byte_count, 0);
    if (byte_count > 0) {
        in.read(reinterpret_cast<char*>(packed.data()),
                static_cast<std::streamsize>(byte_count));
        if (!in)
            throw std::runtime_error("failed to read hit bits");
    }
    arena.hit.resize(static_cast<std::size_t>(count), false);
    arena.hit_count_ = 0;
    for (std::size_t i = 0; i < static_cast<std::size_t>(count); ++i) {
        if (packed[i / 8] & static_cast<std::uint8_t>(1 << (i % 8))) {
            arena.hit[i] = true;
            ++arena.hit_count_;
        }
    }
}

void write_exact_cache_state_file(const std::string& path, const ExactCacheState& state) {
    if (!state.expected_n.has_value())
        throw std::runtime_error("cannot save exact-cache state before n is known");

    std::string tmp_path = path + ".tmp";
    std::ofstream out(tmp_path, std::ios::binary);
    if (!out)
        throw std::runtime_error("could not open temporary state file " + tmp_path);

    out.write(STATE_MAGIC, sizeof(STATE_MAGIC));
    if (!out)
        throw std::runtime_error("failed to write state magic");

    write_u64(out, STATE_VERSION);
    write_u64(out, static_cast<std::uint64_t>(*state.expected_n));
    write_u64(out, static_cast<std::uint64_t>(state.counters.total_lines));
    write_exact_arena(out, state.covered_e);
    write_exact_arena(out, state.seen_p);
    write_hit_bits(out, state.covered_e);
    out.close();
    if (!out)
        throw std::runtime_error("failed to close temporary state file " + tmp_path);

    std::remove(path.c_str());
    if (std::rename(tmp_path.c_str(), path.c_str()) != 0)
        throw std::runtime_error("failed to replace state file " + path);
}

}  // namespace

void ExactCacheState::ensure_arenas(std::size_t n) {
    if (!covered_e.is_initialized())
        covered_e.init(n);
    if (!seen_p.is_initialized())
        seen_p.init(n);
}

int save_exact_cache_state(const std::string& path, const ExactCacheState& state) {
    try {
        write_exact_cache_state_file(path, state);
    }
    catch (const std::exception& err) {
        std::cerr << "[state] save_failed path=" << path
                  << " error=\"" << err.what() << "\""
                  << std::endl;
        return -1;
    }

    std::cerr << "[state] saved path=" << path
              << " total_lines=" << state.counters.total_lines
              << " p_unique=" << state.seen_p.size()
              << " covered_e=" << state.covered_e.size()
              << " hit_e=" << state.covered_e.hit_count()
              << std::endl;
    return 0;
}

int load_exact_cache_state(const std::string& path, ExactCacheState& state) {
    try {
        std::ifstream in(path, std::ios::binary);
        if (!in)
            throw std::runtime_error("could not open state file " + path);

        char magic[sizeof(STATE_MAGIC)];
        in.read(magic, sizeof(magic));
        if (!in || std::memcmp(magic, STATE_MAGIC, sizeof(magic)) != 0)
            throw std::runtime_error("invalid state file magic");

        std::uint64_t version = read_u64(in);
        if (version != STATE_VERSION)
            throw std::runtime_error("unsupported state file version");

        std::uint64_t n = read_u64(in);
        if (n == 0 || n > 255)
            throw std::runtime_error("state has invalid n");

        state.expected_n = static_cast<std::size_t>(n);
        state.counters.total_lines = static_cast<std::size_t>(read_u64(in));

        read_exact_arena(in, state.covered_e, *state.expected_n);
        read_exact_arena(in, state.seen_p, *state.expected_n);
        read_hit_bits(in, state.covered_e);
    }
    catch (const std::exception& err) {
        std::cerr << "[state] load_failed path=" << path
                  << " error=\"" << err.what() << "\""
                  << std::endl;
        return -1;
    }

    std::cerr << "[state] loaded path=" << path
              << " total_lines=" << state.counters.total_lines
              << " p_unique=" << state.seen_p.size()
              << " covered_e=" << state.covered_e.size()
              << " hit_e=" << state.covered_e.hit_count()
              << std::endl;
    return 0;
}

void print_exact_cache_progress(const std::string& path,
                                std::size_t file_lines,
                                const ExactCacheState& state,
                                bool done,
                                const progress::ProgressRate& rate) {
    std::cerr << "[progress]";
    if (done)
        std::cerr << " done";
    std::cerr << " file=" << path
              << " file_lines=" << file_lines
              << " total_lines=" << state.counters.total_lines
              << " p_unique=" << state.seen_p.size()
              << " covered_e=" << state.covered_e.size()
              << " hit_e=" << state.covered_e.hit_count();
    progress::print_progress_rate(std::cerr, rate);
    std::cerr << std::endl;
}
