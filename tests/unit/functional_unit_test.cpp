#include <array>
#include <cstdlib>
#include <iostream>
#include <string_view>

#include "test_log.hpp"
#include "test_support.hpp"
#include "tomasulo/functional_unit.hpp"

#ifndef ALU_TEST_LOG_PATH
#define ALU_TEST_LOG_PATH "riscv_alu_unit_tests.log"
#endif

namespace {

constexpr Address kPc = 0x1000U;

constexpr RobTag make_tag(
    RobIndex index,
    TagGeneration generation = 1U
) {
    return RobTag{index, generation, true};
}

Execute make_execute(
    OP op,
    Word lhs,
    Word rhs,
    Word immediate = 0U,
    Address pc = kPc,
    RobTag tag = make_tag(3U, 2U)
) {
    Execute execute{};
    execute.op = op;
    execute.operand1 = lhs;
    execute.operand2 = rhs;
    execute.immediate = immediate;
    execute.pc = pc;
    execute.tag = tag;
    execute.valid = true;
    return execute;
}

void expect_tag(RobTag actual, RobTag expected) {
    EXPECT_EQ(actual.index, expected.index);
    EXPECT_EQ(actual.generation, expected.generation);
    EXPECT_EQ(actual.valid, expected.valid);
}

void expect_empty_result(const FUResult& result) {
    EXPECT_FALSE(result.valid);
    EXPECT_FALSE(result.tag.valid);
    EXPECT_EQ(result.result, Word{0});
    EXPECT_FALSE(result.branch_taken);
    EXPECT_FALSE(result.has_control_result);
}

FUResult execute_and_latch(Execute execute) {
    FunctionalUnit unit;

    EXPECT_TRUE(unit.can_accept(false));
    EXPECT_TRUE(unit.apply(execute, false, false));
    EXPECT_FALSE(unit.has_result());
    expect_empty_result(unit.result());

    unit.latch();

    EXPECT_TRUE(unit.has_result());
    const FUResult result = unit.result();
    EXPECT_TRUE(result.valid);
    expect_tag(result.tag, execute.tag);
    return result;
}

void test_constructor_starts_empty() {
    FunctionalUnit unit;

    EXPECT_FALSE(unit.has_result());
    EXPECT_TRUE(unit.can_accept(false));
    EXPECT_TRUE(unit.can_accept(true));
    expect_empty_result(unit.result());
}

void test_invalid_execute_is_not_accepted() {
    FunctionalUnit unit;
    Execute execute{};

    EXPECT_FALSE(unit.apply(execute, false, false));
    unit.latch();

    EXPECT_FALSE(unit.has_result());
    expect_empty_result(unit.result());
}

void test_integer_operations() {
    struct Case {
        OP op;
        Word lhs;
        Word rhs;
        Word expected;
    };

    constexpr std::array<Case, 23> cases{{
        {OP::LUI, 0U, 0x12345000U, 0x12345000U},
        {OP::AUIPC, 0x1000U, 0x23456000U, 0x23457000U},
        {OP::ADD, 10U, 20U, 30U},
        {OP::ADDI, 10U, 0xfffffff0U, 0xfffffffaU},
        {OP::ADD, 0xffffffffU, 1U, 0U},
        {OP::SUB, 5U, 8U, 0xfffffffdU},
        {OP::AND, 0xf0f0U, 0x0ff0U, 0x00f0U},
        {OP::ANDI, 0xff00U, 0x0f0fU, 0x0f00U},
        {OP::OR, 0xf000U, 0x0f00U, 0xff00U},
        {OP::ORI, 0x1001U, 0x0110U, 0x1111U},
        {OP::XOR, 0xaaaaU, 0x0f0fU, 0xa5a5U},
        {OP::XORI, 0xffff0000U, 0x00ffff00U, 0xff00ff00U},
        {OP::SLL, 1U, 4U, 16U},
        {OP::SLLI, 1U, 36U, 16U},
        {OP::SRL, 0x80000000U, 4U, 0x08000000U},
        {OP::SRLI, 0x80000000U, 36U, 0x08000000U},
        {OP::SRA, 0x80000000U, 4U, 0xf8000000U},
        {OP::SRAI, 0x80000000U, 36U, 0xf8000000U},
        {OP::SLT, 0xffffffffU, 1U, 1U},
        {OP::SLTI, 1U, 0xffffffffU, 0U},
        {OP::SLTU, 1U, 0xffffffffU, 1U},
        {OP::SLTIU, 0xffffffffU, 1U, 0U},
        {OP::SRA, 0x87654321U, 0U, 0x87654321U},
    }};

    RobIndex tag_index = 1U;
    for (const Case& test_case : cases) {
        Execute execute = make_execute(
            test_case.op,
            test_case.lhs,
            test_case.rhs,
            0U,
            kPc,
            make_tag(tag_index++)
        );
        const FUResult result = execute_and_latch(execute);

        EXPECT_EQ(result.result, test_case.expected);
        EXPECT_EQ(result.actual_next_pc, Address{kPc + 4U});
        EXPECT_FALSE(result.branch_taken);
        EXPECT_FALSE(result.has_control_result);
    }
}

void test_conditional_branches() {
    struct Case {
        OP op;
        Word lhs;
        Word rhs;
        bool taken;
    };

    constexpr std::array<Case, 12> cases{{
        {OP::BEQ, 7U, 7U, true},
        {OP::BEQ, 7U, 8U, false},
        {OP::BNE, 7U, 8U, true},
        {OP::BNE, 7U, 7U, false},
        {OP::BLT, 0xffffffffU, 1U, true},
        {OP::BLT, 1U, 0xffffffffU, false},
        {OP::BGE, 1U, 0xffffffffU, true},
        {OP::BGE, 0xffffffffU, 1U, false},
        {OP::BLTU, 1U, 0xffffffffU, true},
        {OP::BLTU, 0xffffffffU, 1U, false},
        {OP::BGEU, 0xffffffffU, 1U, true},
        {OP::BGEU, 1U, 0xffffffffU, false},
    }};

    constexpr Word offset = 0x20U;
    RobIndex tag_index = 40U;
    for (const Case& test_case : cases) {
        Execute execute = make_execute(
            test_case.op,
            test_case.lhs,
            test_case.rhs,
            offset,
            kPc,
            make_tag(tag_index++)
        );
        const FUResult result = execute_and_latch(execute);

        EXPECT_EQ(result.result, Word{0});
        EXPECT_EQ(result.branch_taken, test_case.taken);
        EXPECT_TRUE(result.has_control_result);
        EXPECT_EQ(
            result.actual_next_pc,
            test_case.taken ? Address{kPc + offset} : Address{kPc + 4U}
        );
    }
}

void test_jumps() {
    Execute jal = make_execute(
        OP::JAL,
        kPc,
        0x40U,
        0x40U,
        kPc,
        make_tag(60U)
    );
    const FUResult jal_result = execute_and_latch(jal);
    EXPECT_EQ(jal_result.result, Word{kPc + 4U});
    EXPECT_EQ(jal_result.actual_next_pc, Address{kPc + 0x40U});
    EXPECT_TRUE(jal_result.branch_taken);
    EXPECT_TRUE(jal_result.has_control_result);

    Execute jalr = make_execute(
        OP::JALR,
        0x2001U,
        0U,
        6U,
        kPc,
        make_tag(61U)
    );
    const FUResult jalr_result = execute_and_latch(jalr);
    EXPECT_EQ(jalr_result.result, Word{kPc + 4U});
    EXPECT_EQ(jalr_result.actual_next_pc, Address{0x2006U});
    EXPECT_TRUE(jalr_result.branch_taken);
    EXPECT_TRUE(jalr_result.has_control_result);
}

void test_backpressure_preserves_old_result() {
    FunctionalUnit unit;
    Execute first = make_execute(OP::ADD, 2U, 3U, 0U, kPc, make_tag(70U));
    EXPECT_TRUE(unit.apply(first, false, false));
    unit.latch();

    EXPECT_TRUE(unit.has_result());
    EXPECT_FALSE(unit.can_accept(false));
    EXPECT_TRUE(unit.can_accept(true));

    Execute second = make_execute(OP::SUB, 9U, 4U, 0U, kPc, make_tag(71U));
    EXPECT_FALSE(unit.apply(second, false, false));
    unit.latch();

    const FUResult result = unit.result();
    EXPECT_EQ(result.result, Word{5U});
    expect_tag(result.tag, first.tag);
}

void test_result_acceptance_clears_output() {
    FunctionalUnit unit;
    Execute execute = make_execute(OP::OR, 1U, 2U);
    EXPECT_TRUE(unit.apply(execute, false, false));
    unit.latch();

    Execute no_execute{};
    EXPECT_FALSE(unit.apply(no_execute, false, true));

    EXPECT_TRUE(unit.has_result());
    unit.latch();

    EXPECT_FALSE(unit.has_result());
    expect_empty_result(unit.result());
}

void test_accept_old_result_and_execute_new_in_same_cycle() {
    FunctionalUnit unit;
    Execute first = make_execute(OP::ADD, 10U, 1U, 0U, kPc, make_tag(80U));
    EXPECT_TRUE(unit.apply(first, false, false));
    unit.latch();

    Execute second = make_execute(OP::SUB, 20U, 3U, 0U, kPc, make_tag(81U));
    EXPECT_TRUE(unit.apply(second, false, true));

    EXPECT_EQ(unit.result().result, Word{11U});
    expect_tag(unit.result().tag, first.tag);

    unit.latch();

    EXPECT_EQ(unit.result().result, Word{17U});
    expect_tag(unit.result().tag, second.tag);
}

void test_flush_has_highest_priority() {
    FunctionalUnit unit;
    Execute old_execute = make_execute(OP::ADD, 4U, 5U);
    EXPECT_TRUE(unit.apply(old_execute, false, false));
    unit.latch();

    Execute new_execute = make_execute(OP::SUB, 9U, 2U);
    EXPECT_FALSE(unit.apply(new_execute, true, true));

    EXPECT_TRUE(unit.has_result());
    unit.latch();

    EXPECT_FALSE(unit.has_result());
    expect_empty_result(unit.result());
}

void test_reset_clears_current_and_pending_result() {
    FunctionalUnit unit;
    Execute first = make_execute(OP::ADD, 1U, 2U);
    EXPECT_TRUE(unit.apply(first, false, false));
    unit.latch();

    Execute pending = make_execute(OP::SUB, 8U, 3U);
    EXPECT_TRUE(unit.apply(pending, false, true));

    unit.reset();
    unit.latch();

    EXPECT_FALSE(unit.has_result());
    expect_empty_result(unit.result());
}

}  // namespace

int main(int argc, char* argv[]) {
    test::TestLog log(ALU_TEST_LOG_PATH);
    if (!log.is_open()) {
        std::cerr << "failed to open ALU test log: "
                  << ALU_TEST_LOG_PATH << '\n';
        return EXIT_FAILURE;
    }
    std::cout << "log: " << ALU_TEST_LOG_PATH << '\n';

    struct TestCase {
        std::string_view name;
        void (*run)();
    };

    const TestCase cases[]{
        {"initialization", test_constructor_starts_empty},
        {"invalid-input", test_invalid_execute_is_not_accepted},
        {"integer", test_integer_operations},
        {"branch", test_conditional_branches},
        {"jump", test_jumps},
        {"backpressure", test_backpressure_preserves_old_result},
        {"accept", test_result_acceptance_clears_output},
        {"throughput", test_accept_old_result_and_execute_new_in_same_cycle},
        {"flush", test_flush_has_highest_priority},
        {"reset", test_reset_clears_current_and_pending_result},
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
        std::cerr << "unknown ALU test case: " << filter << '\n';
        std::cerr << "available cases:";
        for (const TestCase& test_case : cases) {
            std::cerr << ' ' << test_case.name;
        }
        std::cerr << '\n';
        return EXIT_FAILURE;
    }

    if (test::failure_count != 0) {
        std::cerr << test::failure_count << " ALU test(s) failed\n";
        return EXIT_FAILURE;
    }

    std::cout << "all ALU unit tests passed\n";
    return EXIT_SUCCESS;
}
