// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Roman Parpalak and Denis Utkin

#include "word.hpp"

#include <cctype>
#include <sstream>

namespace addline {

namespace {

bool parse_int(const std::string& text, int& out) {
    if (text.empty()) return false;
    int value = 0;
    for (char c : text) {
        if (!std::isdigit(static_cast<unsigned char>(c))) return false;
        value = value * 10 + (c - '0');
        if (value > 1'000'000) return false;
    }
    out = value;
    return true;
}

// "13" -> {13,2}; "13*" -> {13,3}; "13**" -> {13,4}
bool parse_token(const std::string& token, Step& out, std::string& reason) {
    if (token.empty()) {
        reason = "empty token";
        return false;
    }
    if (token.back() == '*') {
        std::size_t head_size = token.size();
        while (head_size > 0 && token[head_size - 1] == '*') --head_size;
        if (!parse_int(token.substr(0, head_size), out.pos)) {
            reason = "bad token: " + token;
            return false;
        }
        out.k = 2 + static_cast<int>(token.size() - head_size);
        return true;
    }
    if (!parse_int(token, out.pos)) {
        reason = "bad token: " + token;
        return false;
    }
    out.k = 2;
    return true;
}

}  // namespace

ParseResult parse_word(const std::string& line) {
    ParseResult result;
    std::string payload = line;

    for (char separator : {')', '|'}) {
        const std::size_t at = payload.find(separator);
        if (at != std::string::npos) {
            payload = payload.substr(at + 1);
            break;
        }
    }

    // A trailing comment, cut after the metadata: addline's own output starts
    // with "#N ... )".
    const std::size_t hash = payload.find('#');
    if (hash != std::string::npos) payload = payload.substr(0, hash);

    std::istringstream stream(payload);
    std::string token;
    while (stream >> token) {
        Step step;
        if (!parse_token(token, step, result.reason)) return result;
        result.word.push_back(step);
    }
    if (result.word.empty()) {
        result.reason = "no generators";
        return result;
    }
    result.ok = true;
    return result;
}

std::string format_word(const Word& word) {
    std::string out;
    for (std::size_t i = 0; i < word.size(); ++i) {
        if (i) out += ' ';
        out += std::to_string(word[i].pos);
        out.append(static_cast<std::size_t>(word[i].k - 2), '*');
    }
    return out;
}

}  // namespace addline
