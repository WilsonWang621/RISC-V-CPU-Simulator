#include <iostream>

#include "tomasulo/cpu.hpp"

int main() {
    CPU cpu{};

    const ImageLoadResult load_result = cpu.load(std::cin);

    if (!load_result.ok()) {
        std::cerr << "failed to load memory image\n";
        return 1;
    }

    const CPU::RunResult result = cpu.run();

    if (!result.halted()) {
        std::cerr
            << "CPU stopped with status "
            << static_cast<int>(result.status)
            << " at pc=0x"
            << std::hex
            << cpu.pc()
            << std::dec
            << '\n';

        return 1;
    }

    std::cout << (result.value & Word{0xffU}) << '\n';

    // 统计输出到 stderr。
    std::cerr
        << "cycles: "
        << cpu.cycle_count()
        << '\n'
        << "committed: "
        << cpu.committed_count()
        << '\n'
        << "branches: "
        << cpu.branch_count()
        << '\n'
        << "correct predictions: "
        << cpu.correct_branch_count()
        << '\n'
        << "accuracy: "
        << cpu.branch_accuracy()
        << '\n';

    return 0;
}
