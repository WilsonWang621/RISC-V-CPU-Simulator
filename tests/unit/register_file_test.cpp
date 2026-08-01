#include <cstdlib>
#include <iostream>
#include <string_view>

#include "test_log.hpp"
#include "test_support.hpp"
#include "tomasulo/register_file.hpp"

namespace {

RegisterWrite make_write(
    RegisterIndex rd,
    Word value,
    bool valid = true
) {
    return RegisterWrite{rd, value, valid};
}

void expect_all_zero(RegisterFile& registers) {
    const auto& values = registers.values();
    for (std::size_t index = 0; index < values.size(); ++index) {
        EXPECT_EQ(values[index], Word{0});
    }
}

void test_constructor_initializes_all_registers_to_zero() {
    RegisterFile registers;

    EXPECT_EQ(registers.values().size(), RegisterFile::kRegisterCount);
    expect_all_zero(registers);
    EXPECT_EQ(registers.read(RegisterIndex{0U}), Word{0});
    EXPECT_EQ(registers.read(RegisterIndex{1U}), Word{0});
    EXPECT_EQ(registers.read(RegisterIndex{31U}), Word{0});
}

void test_commit_becomes_visible_only_after_latch() {
    RegisterFile registers;

    registers.evaluate_commit(make_write(5U, 0x12345678U));

    EXPECT_EQ(registers.read(5U), Word{0});
    EXPECT_EQ(registers.values()[5U], Word{0});

    registers.latch();

    EXPECT_EQ(registers.read(5U), Word{0x12345678U});
    EXPECT_EQ(registers.values()[5U], Word{0x12345678U});
}

void test_x0_ignores_writes() {
    RegisterFile registers;

    registers.evaluate_commit(make_write(0U, 0xffffffffU));
    registers.latch();

    EXPECT_EQ(registers.read(0U), Word{0});
    EXPECT_EQ(registers.values()[0U], Word{0});
}

void test_invalid_register_indices_are_ignored() {
    RegisterFile registers;

    registers.evaluate_commit(make_write(32U, 0x11111111U));
    registers.latch();
    registers.evaluate_commit(make_write(255U, 0x22222222U));
    registers.latch();

    EXPECT_EQ(registers.read(32U), Word{0});
    EXPECT_EQ(registers.read(255U), Word{0});
    expect_all_zero(registers);
}

void test_invalid_commit_does_not_write() {
    RegisterFile registers;

    registers.evaluate_commit(make_write(7U, 0xabcdef01U, false));
    registers.latch();

    EXPECT_EQ(registers.read(7U), Word{0});
}

void test_latest_evaluation_wins_within_a_cycle() {
    RegisterFile registers;

    registers.evaluate_commit(make_write(3U, 10U));
    registers.evaluate_commit(make_write(4U, 20U));
    registers.latch();

    EXPECT_EQ(registers.read(3U), Word{0});
    EXPECT_EQ(registers.read(4U), Word{20U});

    registers.evaluate_commit(make_write(6U, 30U));
    registers.evaluate_commit(RegisterWrite{});
    registers.latch();

    EXPECT_EQ(registers.read(6U), Word{0});
}

void test_latch_consumes_pending_write_once() {
    RegisterFile registers;

    registers.evaluate_commit(make_write(8U, 80U));
    registers.latch();
    registers.latch();

    EXPECT_EQ(registers.read(8U), Word{80U});
    EXPECT_EQ(registers.read(0U), Word{0});
}

void test_reset_clears_committed_and_pending_state() {
    RegisterFile registers;

    registers.evaluate_commit(make_write(1U, 11U));
    registers.latch();
    registers.evaluate_commit(make_write(2U, 22U));

    registers.reset();
    registers.latch();

    expect_all_zero(registers);
}

void test_register_boundaries_can_be_written() {
    RegisterFile registers;

    registers.evaluate_commit(make_write(1U, 0x11111111U));
    registers.latch();
    registers.evaluate_commit(make_write(31U, 0x31313131U));
    registers.latch();

    EXPECT_EQ(registers.read(1U), Word{0x11111111U});
    EXPECT_EQ(registers.read(31U), Word{0x31313131U});
}

}  // namespace

int main(int argc, char* argv[]) {
    test::TestLog log(REGISTER_FILE_TEST_LOG_PATH);
    if (!log.is_open()) {
        std::cerr << "failed to open register file test log: "
                  << REGISTER_FILE_TEST_LOG_PATH << '\n';
        return EXIT_FAILURE;
    }
    std::cout << "log: " << REGISTER_FILE_TEST_LOG_PATH << '\n';

    struct TestCase {
        std::string_view name;
        void (*run)();
    };

    const TestCase cases[]{
        {"initialization", test_constructor_initializes_all_registers_to_zero},
        {"commit", test_commit_becomes_visible_only_after_latch},
        {"x0", test_x0_ignores_writes},
        {"invalid-index", test_invalid_register_indices_are_ignored},
        {"invalid-commit", test_invalid_commit_does_not_write},
        {"overwrite", test_latest_evaluation_wins_within_a_cycle},
        {"consume", test_latch_consumes_pending_write_once},
        {"reset", test_reset_clears_committed_and_pending_state},
        {"boundaries", test_register_boundaries_can_be_written},
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
        std::cerr << "unknown register file test case: " << filter << '\n';
        std::cerr << "available cases:";
        for (const TestCase& test_case : cases) {
            std::cerr << ' ' << test_case.name;
        }
        std::cerr << '\n';
        return EXIT_FAILURE;
    }

    if (test::failure_count != 0) {
        std::cerr << test::failure_count
                  << " register file test(s) failed\n";
        return EXIT_FAILURE;
    }

    std::cout << "all register file unit tests passed\n";
    return EXIT_SUCCESS;
}
