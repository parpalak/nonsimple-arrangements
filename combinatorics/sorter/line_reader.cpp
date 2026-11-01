// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Roman Parpalak and Denis Utkin

#include "line_reader.hpp"

#include <unistd.h>

LineReader::LineReader(const std::string &path, bool follow, int poll_interval_us)
    : file_(path), follow_(follow), poll_interval_us_(poll_interval_us) {}

bool LineReader::getline(std::string &line) {
    if (!follow_) {
        return static_cast<bool>(std::getline(file_, line));
    }

    while (true) {
        if (std::getline(file_, line)) {
            if (file_.eof()) {
                // Got data but no trailing '\n' - possibly incomplete line.
                // Save fragment instead of seeking back.
                partial_ += line;
                file_.clear();
                usleep(poll_interval_us_);
                continue;
            }
            // Complete line with '\n'. Prepend any saved partial data.
            if (!partial_.empty()) {
                line = partial_ + line;
                partial_.clear();
            }
            return true;
        }

        // getline failed.
        if (file_.bad())
            return false;

        // EOF with nothing read — wait for more data.
        file_.clear();
        usleep(poll_interval_us_);
    }
}

LineReader::operator bool() const {
    return file_.good() || (follow_ && !file_.bad());
}
