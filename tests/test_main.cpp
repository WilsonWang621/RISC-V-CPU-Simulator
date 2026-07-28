#include <cstdlib>
#include <iostream>

namespace {

int failures = 0;

void expect_true(bool condition, const char* expression, int line) {
    if (condition) {
        return;
    }

    std::cerr << "test failure at line " << line << ": " << expression << '\n';
    ++failures;
}

}  // namespace

#define EXPECT_TRUE(expression) expect_true((expression), #expression, __LINE__)

int main() {
    // This smoke test verifies that the test target is built and run by CTest.
    // Add component tests here as the loader, decoder, and queues are completed.
    EXPECT_TRUE(sizeof(unsigned char) == 1U);

    if (failures != 0) {
        std::cerr << failures << " test(s) failed\n";
        return EXIT_FAILURE;
    }

    std::cout << "all tests passed\n";
    return EXIT_SUCCESS;
}
