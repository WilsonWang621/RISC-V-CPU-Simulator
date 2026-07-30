#pragma once

#include <iostream>
#include <type_traits>

namespace test {

inline int failure_count = 0;

template <typename Value>
void print_value(const Value& value) {
    if constexpr (std::is_enum_v<Value>) {
        std::cerr << static_cast<std::underlying_type_t<Value>>(value);
    } else if constexpr (std::is_same_v<std::remove_cv_t<Value>, bool>) {
        std::cerr << std::boolalpha << value;
    } else if constexpr (
        std::is_integral_v<Value>
        && sizeof(Value) == sizeof(char)
    ) {
        std::cerr << static_cast<int>(value);
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

    std::cerr << file << ':' << line << ": " << expression << '\n'
              << "  actual:   false\n"
              << "  expected: true\n";
    ++failure_count;
}

inline void expect_false(
    bool condition,
    const char* expression,
    const char* file,
    int line
) {
    if (!condition) {
        return;
    }

    std::cerr << file << ':' << line << ": " << expression << '\n'
              << "  actual:   true\n"
              << "  expected: false\n";
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

    std::cerr << file << ':' << line << ": " << actual_expression << '\n'
              << "  actual:   ";
    print_value(actual);
    std::cerr << '\n'
              << "  expected: ";
    print_value(expected);
    std::cerr << " (" << expected_expression << ")\n";
    ++failure_count;
}

}  // namespace test

#define EXPECT_TRUE(expression) \
    ::test::expect_true((expression), #expression, __FILE__, __LINE__)

#define EXPECT_FALSE(expression) \
    ::test::expect_false((expression), #expression, __FILE__, __LINE__)

#define EXPECT_EQ(actual, expected) \
    ::test::expect_equal( \
        (actual), \
        (expected), \
        #actual, \
        #expected, \
        __FILE__, \
        __LINE__ \
    )
