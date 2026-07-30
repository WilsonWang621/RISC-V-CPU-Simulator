#pragma once

#include <fstream>
#include <iostream>
#include <streambuf>

namespace test {

class TeeBuffer : public std::streambuf {
public:
    TeeBuffer(std::streambuf* first, std::streambuf* second)
        : first_(first), second_(second) {}

protected:
    int overflow(int character) override {
        if (character == traits_type::eof()) {
            return traits_type::not_eof(character);
        }

        const char value = static_cast<char>(character);
        const bool first_ok = first_->sputc(value) != traits_type::eof();
        const bool second_ok = second_->sputc(value) != traits_type::eof();
        return first_ok && second_ok ? character : traits_type::eof();
    }

    int sync() override {
        return first_->pubsync() == 0 && second_->pubsync() == 0 ? 0 : -1;
    }

private:
    std::streambuf* first_;
    std::streambuf* second_;
};

class TestLog {
public:
    explicit TestLog(const char* path)
        : path_(path),
          file_(path),
          stdout_buffer_(std::cout.rdbuf(), file_.rdbuf()),
          stderr_buffer_(std::cerr.rdbuf(), file_.rdbuf()),
          original_stdout_(std::cout.rdbuf(&stdout_buffer_)),
          original_stderr_(std::cerr.rdbuf(&stderr_buffer_)) {}

    ~TestLog() {
        std::cout.flush();
        std::cerr.flush();
        std::cout.rdbuf(original_stdout_);
        std::cerr.rdbuf(original_stderr_);
    }

    TestLog(const TestLog&) = delete;
    TestLog& operator=(const TestLog&) = delete;

    bool is_open() const {
        return file_.is_open();
    }

    const char* path() const {
        return path_;
    }

private:
    const char* path_;
    std::ofstream file_;
    TeeBuffer stdout_buffer_;
    TeeBuffer stderr_buffer_;
    std::streambuf* original_stdout_;
    std::streambuf* original_stderr_;
};

}  // namespace test
