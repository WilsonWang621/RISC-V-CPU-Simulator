#include <iostream>

#include "reference/interpreter.hpp"

namespace {

const char* status_name(Interpreter::Status status) noexcept {
    switch (status) {
        case Interpreter::Status::kNotLoaded:
            return "not loaded";
        case Interpreter::Status::kReady:
            return "ready";
        case Interpreter::Status::kRunning:
            return "running";
        case Interpreter::Status::kHalted:
            return "halted";
        case Interpreter::Status::kLoadError:
            return "load error";
        case Interpreter::Status::kFetchOutOfBounds:
            return "fetch out of bounds";
        case Interpreter::Status::kInvalidInstruction:
            return "invalid instruction";
        case Interpreter::Status::kMemoryOutOfBounds:
            return "memory out of bounds";
    }
    return "unknown";
}

}  // namespace

int main() {
    Interpreter interpreter;
    ImageLoadResult load_result = interpreter.load(std::cin);

    if (!load_result.ok()) {
        std::cerr << "Failed to load memory image"
                  << " (status=" << static_cast<int>(load_result.status)
                  << ", line=" << load_result.error_line
                  << ", token=\"" << load_result.error_token << "\")\n";
        return 1;
    }

    Interpreter::StepResult result = interpreter.run();
    if (result.halted()) {
        std::cout << (result.value & Word{0xff}) << '\n';
        return 0;
    }

    std::cerr << "Interpreter stopped with " << status_name(result.status)
              << " at pc=0x" << std::hex << interpreter.pc() << std::dec
              << " after " << interpreter.instruction_count()
              << " instructions\n";
    return 1;
}
