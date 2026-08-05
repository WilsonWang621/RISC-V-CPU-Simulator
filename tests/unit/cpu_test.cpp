#include <cstddef>
#include <cstdlib>
#include <iomanip>
#include <initializer_list>
#include <iostream>
#include <sstream>
#include <string>
#include <string_view>

#include "test_log.hpp"
#include "test_support.hpp"
#include "tomasulo/cpu.hpp"

#ifndef CPU_TEST_LOG_PATH
#define CPU_TEST_LOG_PATH "riscv_cpu_unit_tests.log"
#endif

namespace {

constexpr InstructionBits encode_i(
    int immediate,
    RegisterIndex rs1,
    Word funct3,
    RegisterIndex rd,
    Word opcode
) {
    const Word immediate_bits = static_cast<Word>(immediate) & 0xfffU;
    return static_cast<InstructionBits>(
        (immediate_bits << 20U)
        | (static_cast<Word>(rs1) << 15U)
        | (funct3 << 12U)
        | (static_cast<Word>(rd) << 7U)
        | opcode
    );
}

constexpr InstructionBits encode_r(
    Word funct7,
    RegisterIndex rs2,
    RegisterIndex rs1,
    Word funct3,
    RegisterIndex rd
) {
    return static_cast<InstructionBits>(
        (funct7 << 25U)
        | (static_cast<Word>(rs2) << 20U)
        | (static_cast<Word>(rs1) << 15U)
        | (funct3 << 12U)
        | (static_cast<Word>(rd) << 7U)
        | 0x33U
    );
}

constexpr InstructionBits encode_s(
    int immediate,
    RegisterIndex rs2,
    RegisterIndex rs1,
    Word funct3
) {
    const Word immediate_bits = static_cast<Word>(immediate) & 0xfffU;
    return static_cast<InstructionBits>(
        ((immediate_bits >> 5U) << 25U)
        | (static_cast<Word>(rs2) << 20U)
        | (static_cast<Word>(rs1) << 15U)
        | (funct3 << 12U)
        | ((immediate_bits & 0x1fU) << 7U)
        | 0x23U
    );
}

constexpr InstructionBits encode_b(
    int immediate,
    RegisterIndex rs2,
    RegisterIndex rs1,
    Word funct3
) {
    const Word bits = static_cast<Word>(immediate) & 0x1fffU;
    return static_cast<InstructionBits>(
        (((bits >> 12U) & 0x1U) << 31U)
        | (((bits >> 5U) & 0x3fU) << 25U)
        | (static_cast<Word>(rs2) << 20U)
        | (static_cast<Word>(rs1) << 15U)
        | (funct3 << 12U)
        | (((bits >> 1U) & 0xfU) << 8U)
        | (((bits >> 11U) & 0x1U) << 7U)
        | 0x63U
    );
}

constexpr InstructionBits encode_j(
    int immediate,
    RegisterIndex rd
) {
    const Word bits = static_cast<Word>(immediate) & 0x1fffffU;
    return static_cast<InstructionBits>(
        (((bits >> 20U) & 0x1U) << 31U)
        | (((bits >> 1U) & 0x3ffU) << 21U)
        | (((bits >> 11U) & 0x1U) << 20U)
        | (((bits >> 12U) & 0xffU) << 12U)
        | (static_cast<Word>(rd) << 7U)
        | 0x6fU
    );
}

constexpr InstructionBits addi(
    RegisterIndex rd,
    RegisterIndex rs1,
    int immediate
) {
    return encode_i(immediate, rs1, 0U, rd, 0x13U);
}

constexpr InstructionBits add(
    RegisterIndex rd,
    RegisterIndex rs1,
    RegisterIndex rs2
) {
    return encode_r(0U, rs2, rs1, 0U, rd);
}

constexpr InstructionBits lw(
    RegisterIndex rd,
    RegisterIndex rs1,
    int immediate
) {
    return encode_i(immediate, rs1, 2U, rd, 0x03U);
}

constexpr InstructionBits sw(
    RegisterIndex rs2,
    RegisterIndex rs1,
    int immediate
) {
    return encode_s(immediate, rs2, rs1, 2U);
}

constexpr InstructionBits beq(
    RegisterIndex rs1,
    RegisterIndex rs2,
    int immediate
) {
    return encode_b(immediate, rs2, rs1, 0U);
}

constexpr InstructionBits jal(RegisterIndex rd, int immediate) {
    return encode_j(immediate, rd);
}

constexpr InstructionBits jalr(
    RegisterIndex rd,
    RegisterIndex rs1,
    int immediate
) {
    return encode_i(immediate, rs1, 0U, rd, 0x67U);
}

constexpr InstructionBits kHalt = 0x0ff00513U;
constexpr CycleCount kMaximumCycles = CycleCount{2'000};

std::string make_image(
    std::initializer_list<InstructionBits> instructions
) {
    std::ostringstream image;
    image << "@00000000" << std::hex << std::setfill('0');

    for (const InstructionBits instruction : instructions) {
        for (unsigned shift = 0U; shift < 32U; shift += 8U) {
            const Word byte = (instruction >> shift) & 0xffU;
            image << ' ' << std::setw(2) << byte;
        }
    }
    image << '\n';
    return image.str();
}

ImageLoadResult load_program(
    CPU& cpu,
    std::initializer_list<InstructionBits> instructions
) {
    std::istringstream input(make_image(instructions));
    return cpu.load(input);
}

CPU::RunResult run_bounded(CPU& cpu) {
    for (
        CycleCount cycle = 0U;
        cycle < kMaximumCycles && cpu.status() == CPU::Status::Running;
        ++cycle
    ) {
        cpu.cycle();
    }

    if (cpu.status() == CPU::Status::Running) {
        return CPU::RunResult{CPU::Status::Running, Word{0}};
    }
    return cpu.run();
}

void expect_halted_with(CPU& cpu, Word expected_value) {
    const CPU::RunResult result = run_bounded(cpu);
    EXPECT_EQ(result.status, CPU::Status::Halted);
    EXPECT_TRUE(result.halted());
    EXPECT_FALSE(result.failed());
    EXPECT_EQ(result.value, expected_value);
    EXPECT_TRUE(cpu.cycle_count() <= kMaximumCycles);
}

void test_constructor_is_not_loaded() {
    CPU cpu;

    EXPECT_EQ(cpu.status(), CPU::Status::NotLoaded);
    EXPECT_EQ(cpu.pc(), Address{0});
    EXPECT_EQ(cpu.cycle_count(), CycleCount{0});
    EXPECT_EQ(cpu.committed_count(), CycleCount{0});
    EXPECT_EQ(cpu.branch_count(), CycleCount{0});
    EXPECT_EQ(cpu.correct_branch_count(), CycleCount{0});
    EXPECT_EQ(cpu.branch_accuracy(), 0.0);
    EXPECT_EQ(cpu.register_value(RegisterIndex{0}), Word{0});

    cpu.cycle();
    EXPECT_EQ(cpu.status(), CPU::Status::NotLoaded);
    EXPECT_EQ(cpu.cycle_count(), CycleCount{0});
}

void test_load_error_is_reported() {
    CPU cpu;
    std::istringstream input("01 02 03 04\n");

    const ImageLoadResult load_result = cpu.load(input);
    EXPECT_FALSE(load_result.ok());
    EXPECT_EQ(cpu.status(), CPU::Status::LoadError);

    const CPU::RunResult result = cpu.run();
    EXPECT_EQ(result.status, CPU::Status::LoadError);
    EXPECT_TRUE(result.failed());
    EXPECT_FALSE(result.halted());
}

void test_simple_addi_and_halt() {
    CPU cpu;
    ImageLoadResult loaded = load_program(
        cpu,
        {addi(10U, 0U, 42), kHalt}
    );
    EXPECT_TRUE(loaded.ok());
    EXPECT_EQ(cpu.status(), CPU::Status::Running);

    expect_halted_with(cpu, Word{42});
    EXPECT_EQ(cpu.register_value(RegisterIndex{10}), Word{42});
    EXPECT_EQ(cpu.committed_count(), CycleCount{1});
    EXPECT_EQ(cpu.branch_count(), CycleCount{0});
}

void test_register_dependencies_are_forwarded() {
    CPU cpu;
    ImageLoadResult loaded = load_program(
        cpu,
        {
            addi(1U, 0U, 7),
            addi(2U, 1U, 5),
            add(10U, 1U, 2U),
            kHalt,
        }
    );
    EXPECT_TRUE(loaded.ok());

    expect_halted_with(cpu, Word{19});
    EXPECT_EQ(cpu.register_value(RegisterIndex{1}), Word{7});
    EXPECT_EQ(cpu.register_value(RegisterIndex{2}), Word{12});
    EXPECT_EQ(cpu.register_value(RegisterIndex{10}), Word{19});
    EXPECT_EQ(cpu.committed_count(), CycleCount{3});
}

void test_x0_remains_zero() {
    CPU cpu;
    ImageLoadResult loaded = load_program(
        cpu,
        {
            addi(0U, 0U, 123),
            addi(10U, 0U, 5),
            kHalt,
        }
    );
    EXPECT_TRUE(loaded.ok());

    expect_halted_with(cpu, Word{5});
    EXPECT_EQ(cpu.register_value(RegisterIndex{0}), Word{0});
}

void test_taken_branch_flushes_wrong_path() {
    CPU cpu;
    ImageLoadResult loaded = load_program(
        cpu,
        {
            addi(1U, 0U, 1),
            beq(1U, 1U, 8),
            addi(10U, 0U, 99),
            addi(10U, 0U, 42),
            kHalt,
        }
    );
    EXPECT_TRUE(loaded.ok());

    expect_halted_with(cpu, Word{42});
    EXPECT_EQ(cpu.register_value(RegisterIndex{10}), Word{42});
    EXPECT_EQ(cpu.branch_count(), CycleCount{1});
    EXPECT_EQ(cpu.correct_branch_count(), CycleCount{0});
    EXPECT_EQ(cpu.branch_accuracy(), 0.0);
}

void test_jal_redirects_and_writes_link_register() {
    CPU cpu;
    ImageLoadResult loaded = load_program(
        cpu,
        {
            jal(1U, 8),
            addi(10U, 0U, 99),
            addi(10U, 0U, 42),
            kHalt,
        }
    );
    EXPECT_TRUE(loaded.ok());

    expect_halted_with(cpu, Word{42});
    EXPECT_EQ(cpu.register_value(RegisterIndex{1}), Word{4});
    EXPECT_EQ(cpu.register_value(RegisterIndex{10}), Word{42});
}

void test_jalr_discards_invalid_wrong_path() {
    CPU cpu;
    ImageLoadResult loaded = load_program(
        cpu,
        {
            addi(1U, 0U, 16),
            jalr(0U, 1U, 0),
            0xffffffffU,
            0xffffffffU,
            addi(10U, 0U, 42),
            kHalt,
        }
    );
    EXPECT_TRUE(loaded.ok());

    expect_halted_with(cpu, Word{42});
    EXPECT_EQ(cpu.register_value(RegisterIndex{10}), Word{42});
}

void test_store_then_load_round_trip() {
    CPU cpu;
    ImageLoadResult loaded = load_program(
        cpu,
        {
            addi(1U, 0U, 0x100),
            addi(2U, 0U, 0x5a),
            sw(2U, 1U, 0),
            lw(10U, 1U, 0),
            kHalt,
        }
    );
    EXPECT_TRUE(loaded.ok());

    expect_halted_with(cpu, Word{0x5a});
    EXPECT_EQ(cpu.register_value(RegisterIndex{10}), Word{0x5a});
    EXPECT_EQ(cpu.committed_count(), CycleCount{4});
}

void test_invalid_instruction_stops_cpu() {
    CPU cpu;
    ImageLoadResult loaded = load_program(cpu, {0xffffffffU});
    EXPECT_TRUE(loaded.ok());

    const CPU::RunResult result = run_bounded(cpu);
    EXPECT_EQ(result.status, CPU::Status::InvalidInstruction);
    EXPECT_TRUE(result.failed());
    EXPECT_FALSE(result.halted());
}

void test_reload_resets_architectural_and_statistical_state() {
    CPU cpu;
    ImageLoadResult first_load = load_program(
        cpu,
        {addi(10U, 0U, 42), kHalt}
    );
    EXPECT_TRUE(first_load.ok());
    expect_halted_with(cpu, Word{42});
    EXPECT_TRUE(cpu.cycle_count() > CycleCount{0});
    EXPECT_EQ(cpu.register_value(RegisterIndex{10}), Word{42});

    ImageLoadResult second_load = load_program(
        cpu,
        {addi(10U, 0U, 7), kHalt}
    );
    EXPECT_TRUE(second_load.ok());
    EXPECT_EQ(cpu.status(), CPU::Status::Running);
    EXPECT_EQ(cpu.pc(), Address{0});
    EXPECT_EQ(cpu.cycle_count(), CycleCount{0});
    EXPECT_EQ(cpu.committed_count(), CycleCount{0});
    EXPECT_EQ(cpu.branch_count(), CycleCount{0});
    EXPECT_EQ(cpu.correct_branch_count(), CycleCount{0});
    EXPECT_EQ(cpu.register_value(RegisterIndex{10}), Word{0});

    expect_halted_with(cpu, Word{7});
}

}  // namespace

int main(int argc, char* argv[]) {
    test::TestLog log(CPU_TEST_LOG_PATH);
    if (!log.is_open()) {
        std::cerr << "failed to open CPU test log: "
                  << CPU_TEST_LOG_PATH << '\n';
        return EXIT_FAILURE;
    }
    std::cout << "log: " << CPU_TEST_LOG_PATH << '\n';

    struct TestCase {
        std::string_view name;
        void (*run)();
    };

    const TestCase cases[]{
        {"initialization", test_constructor_is_not_loaded},
        {"load-error", test_load_error_is_reported},
        {"simple", test_simple_addi_and_halt},
        {"dependencies", test_register_dependencies_are_forwarded},
        {"x0", test_x0_remains_zero},
        {"branch-flush", test_taken_branch_flushes_wrong_path},
        {"jal", test_jal_redirects_and_writes_link_register},
        {"jalr-wrong-path", test_jalr_discards_invalid_wrong_path},
        {"memory", test_store_then_load_round_trip},
        {"invalid", test_invalid_instruction_stops_cpu},
        {"reload", test_reload_resets_architectural_and_statistical_state},
    };

    const std::string_view filter = argc > 1 ? argv[1] : "";
    bool ran_test = false;
    for (const TestCase& test_case : cases) {
        if (!filter.empty() && filter != test_case.name) {
            continue;
        }

        ran_test = true;
        std::cout << "[ RUN      ] " << test_case.name << '\n';
        test_case.run();
    }

    if (!ran_test) {
        std::cerr << "unknown CPU test case: " << filter << '\n';
        std::cerr << "available cases:";
        for (const TestCase& test_case : cases) {
            std::cerr << ' ' << test_case.name;
        }
        std::cerr << '\n';
        return EXIT_FAILURE;
    }

    if (test::failure_count != 0) {
        std::cerr << test::failure_count << " CPU test(s) failed\n";
        return EXIT_FAILURE;
    }

    std::cout << "all CPU unit tests passed\n";
    return EXIT_SUCCESS;
}
