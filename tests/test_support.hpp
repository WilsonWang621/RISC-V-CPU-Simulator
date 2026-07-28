#pragma once

#include <iostream>
#include <type_traits>

namespace test {

inline int failure_count = 0;

template <typename Value>
void print_value(const Value& value) {
    if constexpr (std::is_enum_v<Value>) {
        std::cerr << static_cast<std::underlying_type_t<Value>>(value);
    } else {
        std::cerr << value;
    }
}

inline void expect_true(
    bool condition,
    const char* expression,
    const char* file,
    int line
) {
    if (condition) {
        return;
    }

    std::cerr << file << ':' << line
              << ": expected true: " << expression << '\n';
    ++failure_count;
}

template <typename Actual, typename Expected>
void expect_equal(
    const Actual& actual,
    const Expected& expected,
    const char* actual_expression,
    const char* expected_expression,
    const char* file,
    int line
) {
    if (actual == expected) {
        return;
    }

    std::cerr << file << ':' << line
              << ": expected " << actual_expression
              << " == " << expected_expression
              << ", but got ";
    print_value(actual);
    std::cerr << " and ";
    print_value(expected);
    std::cerr << '\n';
    ++failure_count;
}

}  // namespace test

#define EXPECT_TRUE(expression) \
    ::test::expect_true((expression), #expression, __FILE__, __LINE__)

#define EXPECT_FALSE(expression) \
    ::test::expect_true(!(expression), "!(" #expression ")", __FILE__, __LINE__)

#define EXPECT_EQ(actual, expected) \
    ::test::expect_equal( \
        (actual), \
        (expected), \
        #actual, \
        #expected, \
        __FILE__, \
        __LINE__ \
    )
