#include <array>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <string>

#include "reference/interpreter.hpp"

namespace {

struct DataCase {
    const char* relative_path;
    Word expected_exit_code;
};

constexpr std::array<DataCase, 19> kDataCases{{
    {"data/sample/sample.data", Word{94}},
    {"data/testcases/array_test1.data", Word{123}},
    {"data/testcases/array_test2.data", Word{43}},
    {"data/testcases/basicopt1.data", Word{88}},
    {"data/testcases/bulgarian.data", Word{159}},
    {"data/testcases/expr.data", Word{58}},
    {"data/testcases/gcd.data", Word{178}},
    {"data/testcases/hanoi.data", Word{20}},
    {"data/testcases/lvalue2.data", Word{175}},
    {"data/testcases/magic.data", Word{106}},
    {"data/testcases/manyarguments.data", Word{40}},
    {"data/testcases/multiarray.data", Word{115}},
    {"data/testcases/naive.data", Word{94}},
    {"data/testcases/pi.data", Word{137}},
    {"data/testcases/qsort.data", Word{105}},
    {"data/testcases/queens.data", Word{171}},
    {"data/testcases/statement_test.data", Word{50}},
    {"data/testcases/superloop.data", Word{134}},
    {"data/testcases/tak.data", Word{186}},
}};

// This guard catches accidental infinite loops while still leaving ample room
// for the larger recursive and sorting programs in data/testcases.
constexpr CycleCount kMaximumInstructions = CycleCount{500'000'000};

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

void print_failure_prefix(const DataCase& data_case) {
    std::cerr << "[FAIL] " << data_case.relative_path << ": ";
}

bool run_case(const DataCase& data_case) {
    const std::string path =
        std::string{RISCV_SOURCE_DIR} + '/' + data_case.relative_path;
    std::ifstream input{path};

    if (!input.is_open()) {
        print_failure_prefix(data_case);
        std::cerr << "cannot open input file: " << path << '\n';
        return false;
    }

    Interpreter interpreter;
    ImageLoadResult load_result = interpreter.load(input);
    if (!load_result.ok()) {
        print_failure_prefix(data_case);
        std::cerr << "image load failed"
                  << " (status=" << static_cast<int>(load_result.status)
                  << ", line=" << load_result.error_line
                  << ", token=\"" << load_result.error_token << "\")\n";
        return false;
    }

    for (;;) {
        const Interpreter::Status before_status = interpreter.status();
        const Address before_pc = interpreter.pc();
        const CycleCount before_count = interpreter.instruction_count();

        Interpreter::StepResult result = interpreter.step();

        if (result.status == Interpreter::Status::kHalted) {
            const Word actual = result.value & Word{0xff};
            if (actual != data_case.expected_exit_code) {
                print_failure_prefix(data_case);
                std::cerr << "wrong exit code"
                          << " (expected=" << data_case.expected_exit_code
                          << ", actual=" << actual
                          << ", pc=0x" << std::hex << interpreter.pc()
                          << std::dec
                          << ", instructions=" << interpreter.instruction_count()
                          << ")\n";
                return false;
            }

            std::cout << "[PASS] " << data_case.relative_path
                      << " => " << actual
                      << " (" << interpreter.instruction_count()
                      << " instructions)\n";
            return true;
        }

        if (result.failed()) {
            print_failure_prefix(data_case);
            std::cerr << status_name(result.status)
                      << " at pc=0x" << std::hex << interpreter.pc()
                      << std::dec
                      << " after " << interpreter.instruction_count()
                      << " instructions\n";
            return false;
        }

        const bool made_progress =
            interpreter.pc() != before_pc ||
            interpreter.instruction_count() != before_count ||
            interpreter.status() != before_status;
        if (!made_progress) {
            print_failure_prefix(data_case);
            std::cerr << "interpreter made no progress"
                      << " (status=" << status_name(interpreter.status())
                      << ", pc=0x" << std::hex << interpreter.pc()
                      << std::dec
                      << ", instructions=" << interpreter.instruction_count()
                      << ")\n";
            return false;
        }

        if (interpreter.instruction_count() >= kMaximumInstructions) {
            print_failure_prefix(data_case);
            std::cerr << "instruction limit exceeded"
                      << " (limit=" << kMaximumInstructions
                      << ", status=" << status_name(interpreter.status())
                      << ", pc=0x" << std::hex << interpreter.pc()
                      << std::dec << ")\n";
            return false;
        }
    }
}

}  // namespace

int main() {
    std::size_t passed = 0;

    for (const DataCase& data_case : kDataCases) {
        if (run_case(data_case)) {
            ++passed;
        }
    }

    const std::size_t failed = kDataCases.size() - passed;
    std::cout << "\nInterpreter data tests: "
              << passed << " passed, " << failed << " failed\n";

    return failed == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
