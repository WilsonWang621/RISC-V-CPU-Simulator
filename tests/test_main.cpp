#include <cstdlib>
#include <iostream>

#include "test_support.hpp"

void run_queue_tests();
void run_type_tests();

int main() {
    run_type_tests();
    run_queue_tests();

    if (test::failure_count != 0) {
        std::cerr << test::failure_count << " test(s) failed\n";
        return EXIT_FAILURE;
    }

    std::cout << "all tests passed\n";
    return EXIT_SUCCESS;
}
