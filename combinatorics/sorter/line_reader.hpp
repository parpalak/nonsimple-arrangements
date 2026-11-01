// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Roman Parpalak and Denis Utkin

#pragma once

#include <fstream>
#include <string>

class LineReader {
    std::ifstream file_;
    bool follow_;
    int poll_interval_us_;
    std::string partial_;

public:
    explicit LineReader(const std::string &path, bool follow = false, int poll_interval_us = 50000);

    bool getline(std::string &line);

    explicit operator bool() const;
};
