#include <iostream>

int main() {
    // The simulator will eventually load the memory image from standard input.
    // Keeping the entry point minimal lets the project build while the CPU
    // modules are implemented incrementally.
    return std::cin.bad() ? 1 : 0;
}
