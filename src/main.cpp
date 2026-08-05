#include <iostream>

#include "reference/interpreter.hpp"
#include "tomasulo/cpu.hpp"

// namespace {

// const char* status_name(Interpreter::Status status) noexcept {
//     switch (status) {
//         case Interpreter::Status::kNotLoaded:
//             return "not loaded";
//         case Interpreter::Status::kReady:
//             return "ready";
//         case Interpreter::Status::kRunning:
//             return "running";
//         case Interpreter::Status::kHalted:
//             return "halted";
//         case Interpreter::Status::kLoadError:
//             return "load error";
//         case Interpreter::Status::kFetchOutOfBounds:
//             return "fetch out of bounds";
//         case Interpreter::Status::kInvalidInstruction:
//             return "invalid instruction";
//         case Interpreter::Status::kMemoryOutOfBounds:
//             return "memory out of bounds";
//     }
//     return "unknown";
// }

// }  // namespace

int main() {
    // Interpreter interpreter;
    // ImageLoadResult load_result = interpreter.load(std::cin);

    // if (!load_result.ok()) {
    //     std::cerr << "Failed to load memory image"
    //               << " (status=" << static_cast<int>(load_result.status)
    //               << ", line=" << load_result.error_line
    //               << ", token=\"" << load_result.error_token << "\")\n";
    //     return 1;
    // }

    // Interpreter::StepResult result = interpreter.run();
    // if (result.halted()) {
    //     std::cout << (result.value & Word{0xff}) << '\n';
    //     return 0;
    // }

    // std::cerr << "Interpreter stopped with " << status_name(result.status)
    //           << " at pc=0x" << std::hex << interpreter.pc() << std::dec
    //           << " after " << interpreter.instruction_count()
    //           << " instructions\n";
    // return 1;

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
