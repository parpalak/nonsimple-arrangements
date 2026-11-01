// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Roman Parpalak and Denis Utkin

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include "line_reader.hpp"

#include <cstdio>
#include <fstream>
#include <string>
#include <thread>
#include <chrono>
#include <atomic>

namespace {

class TempFile {
    std::string path_;
public:
    TempFile() {
        char tmpl[] = "/tmp/line_reader_test_XXXXXX";
        int fd = mkstemp(tmpl);
        if (fd >= 0)
            close(fd);
        path_ = tmpl;
    }
    ~TempFile() { std::remove(path_.c_str()); }

    const std::string &path() const { return path_; }

    void write(const std::string &data) {
        std::ofstream f(path_, std::ios::app | std::ios::binary);
        f << data;
        f.flush();
    }
};

} // namespace

TEST_CASE("reads complete file like std::getline") {
    TempFile tmp;
    tmp.write("line1\nline2\nline3\n");

    LineReader reader(tmp.path());
    std::string line;
    std::vector<std::string> lines;
    while (reader.getline(line))
        lines.push_back(line);

    CHECK(lines == std::vector<std::string>{"line1", "line2", "line3"});
}

TEST_CASE("returns last line without trailing newline in non-follow mode") {
    TempFile tmp;
    tmp.write("line1\nline2");

    LineReader reader(tmp.path());
    std::string line;
    std::vector<std::string> lines;
    while (reader.getline(line))
        lines.push_back(line);

    CHECK(lines == std::vector<std::string>{"line1", "line2"});
}

TEST_CASE("empty file returns nothing in non-follow mode") {
    TempFile tmp;

    LineReader reader(tmp.path());
    std::string line;
    CHECK_FALSE(reader.getline(line));
}

TEST_CASE("follow mode waits for incomplete line to be completed") {
    TempFile tmp;
    tmp.write("complete\npartial");

    std::atomic<bool> done{false};
    std::vector<std::string> lines;

    std::thread reader_thread([&]() {
        LineReader reader(tmp.path(), true, 10000);
        std::string line;
        while (reader.getline(line)) {
            lines.push_back(line);
            if (lines.size() == 2)
                break;
        }
        done = true;
    });

    // Give reader time to hit the incomplete line and block.
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    CHECK(lines.size() == 1);
    CHECK(lines[0] == "complete");

    // Complete the partial line.
    tmp.write("\n");

    reader_thread.join();
    CHECK(done);
    CHECK(lines.size() == 2);
    CHECK(lines[1] == "partial");
}

TEST_CASE("follow mode picks up lines appended to an empty file") {
    TempFile tmp;

    std::atomic<bool> done{false};
    std::vector<std::string> lines;

    std::thread reader_thread([&]() {
        LineReader reader(tmp.path(), true, 10000);
        std::string line;
        while (reader.getline(line)) {
            lines.push_back(line);
            if (lines.size() == 1)
                break;
        }
        done = true;
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    CHECK(lines.empty());

    tmp.write("hello\n");

    reader_thread.join();
    CHECK(done);
    CHECK(lines.size() == 1);
    CHECK(lines[0] == "hello");
}

TEST_CASE("follow mode picks up multiple lines appended in one write") {
    TempFile tmp;
    tmp.write("first\n");

    std::vector<std::string> lines;

    std::thread reader_thread([&]() {
        LineReader reader(tmp.path(), true, 10000);
        std::string line;
        while (reader.getline(line)) {
            lines.push_back(line);
            if (lines.size() == 3)
                break;
        }
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    tmp.write("second\nthird\n");

    reader_thread.join();
    CHECK(lines == std::vector<std::string>{"first", "second", "third"});
}
