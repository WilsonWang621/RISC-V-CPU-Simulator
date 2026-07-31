#include <cstdlib>
#include <iostream>
#include <string_view>

#include "test_log.hpp"
#include "test_support.hpp"
#include "tomasulo/rename_table.hpp"

#ifndef RENAME_TABLE_TEST_LOG_PATH
#define RENAME_TABLE_TEST_LOG_PATH "riscv_rename_table_unit_tests.log"
#endif

namespace {

constexpr RobTag make_tag(
    RobIndex index,
    TagGeneration generation = 1U
) {
    return RobTag{index, generation, true};
}

RATWrite make_issue(
    RegisterIndex rd,
    RobTag tag,
    bool valid = true
) {
    return RATWrite{rd, tag, valid};
}

RATCommit make_commit(
    RegisterIndex rd,
    RobTag tag,
    bool valid = true
) {
    return RATCommit{rd, tag, valid};
}

void expect_tag(RobTag actual, RobTag expected) {
    EXPECT_EQ(actual.index, expected.index);
    EXPECT_EQ(actual.generation, expected.generation);
    EXPECT_EQ(actual.valid, expected.valid);
}

void expect_unmapped(RenameTable& table, RegisterIndex reg) {
    expect_tag(table.lookup(reg), RobTag{});
    EXPECT_FALSE(table.busy(reg));
}

void issue_and_latch(
    RenameTable& table,
    RegisterIndex rd,
    RobTag tag
) {
    table.evaluate_updates(make_issue(rd, tag), RATCommit{}, false);
    table.latch();
}

void test_constructor_initializes_all_registers_unmapped() {
    RenameTable table;

    for (RegisterIndex reg = 0U; reg < 32U; ++reg) {
        expect_unmapped(table, reg);
    }
    expect_unmapped(table, RegisterIndex{32U});
    expect_unmapped(table, RegisterIndex{255U});
}

void test_issue_mapping_is_visible_only_after_latch() {
    RenameTable table;
    const RobTag producer = make_tag(3U, 7U);

    table.evaluate_updates(
        make_issue(5U, producer),
        RATCommit{},
        false
    );

    expect_unmapped(table, RegisterIndex{5U});

    table.latch();

    expect_tag(table.lookup(5U), producer);
    EXPECT_TRUE(table.busy(5U));
}

void test_invalid_issue_updates_are_ignored() {
    RenameTable table;

    table.evaluate_updates(
        make_issue(4U, make_tag(1U), false),
        RATCommit{},
        false
    );
    table.latch();
    expect_unmapped(table, RegisterIndex{4U});

    table.evaluate_updates(
        make_issue(4U, RobTag{}, true),
        RATCommit{},
        false
    );
    table.latch();
    expect_unmapped(table, RegisterIndex{4U});

    table.evaluate_updates(
        make_issue(0U, make_tag(2U), true),
        RATCommit{},
        false
    );
    table.latch();
    expect_unmapped(table, RegisterIndex{0U});

    table.evaluate_updates(
        make_issue(32U, make_tag(3U), true),
        RATCommit{},
        false
    );
    table.latch();
    expect_unmapped(table, RegisterIndex{32U});
}

void test_matching_commit_clears_mapping_after_latch() {
    RenameTable table;
    const RobTag producer = make_tag(6U, 2U);
    issue_and_latch(table, 9U, producer);

    table.evaluate_updates(
        RATWrite{},
        make_commit(9U, producer),
        false
    );

    expect_tag(table.lookup(9U), producer);

    table.latch();

    expect_unmapped(table, RegisterIndex{9U});
}

void test_stale_commit_does_not_clear_younger_mapping() {
    RenameTable table;
    const RobTag younger = make_tag(4U, 8U);
    const RobTag stale_generation = make_tag(4U, 7U);
    issue_and_latch(table, 10U, younger);

    table.evaluate_updates(
        RATWrite{},
        make_commit(10U, stale_generation),
        false
    );
    table.latch();

    expect_tag(table.lookup(10U), younger);
    EXPECT_TRUE(table.busy(10U));
}

void test_invalid_commit_is_ignored() {
    RenameTable table;
    const RobTag producer = make_tag(7U, 3U);
    issue_and_latch(table, 11U, producer);

    table.evaluate_updates(
        RATWrite{},
        make_commit(11U, producer, false),
        false
    );
    table.latch();
    expect_tag(table.lookup(11U), producer);

    table.evaluate_updates(
        RATWrite{},
        make_commit(11U, RobTag{}, true),
        false
    );
    table.latch();
    expect_tag(table.lookup(11U), producer);
}

void test_issue_wins_over_same_cycle_commit() {
    RenameTable table;
    const RobTag older = make_tag(1U, 1U);
    const RobTag younger = make_tag(2U, 1U);
    issue_and_latch(table, 12U, older);

    table.evaluate_updates(
        make_issue(12U, younger),
        make_commit(12U, older),
        false
    );
    table.latch();

    expect_tag(table.lookup(12U), younger);
    EXPECT_TRUE(table.busy(12U));
}

void test_updates_preserve_unrelated_mappings() {
    RenameTable table;
    const RobTag first = make_tag(8U, 1U);
    const RobTag second = make_tag(9U, 1U);
    issue_and_latch(table, 13U, first);
    issue_and_latch(table, 14U, second);

    table.evaluate_updates(
        RATWrite{},
        make_commit(13U, first),
        false
    );
    table.latch();

    expect_unmapped(table, RegisterIndex{13U});
    expect_tag(table.lookup(14U), second);
}

void test_flush_has_highest_priority_and_clears_all_mappings() {
    RenameTable table;
    const RobTag first = make_tag(10U, 2U);
    const RobTag second = make_tag(11U, 2U);
    issue_and_latch(table, 15U, first);
    issue_and_latch(table, 16U, second);

    table.evaluate_updates(
        make_issue(17U, make_tag(12U, 2U)),
        make_commit(15U, first),
        true
    );

    expect_tag(table.lookup(15U), first);
    expect_tag(table.lookup(16U), second);

    table.latch();

    for (RegisterIndex reg = 0U; reg < 32U; ++reg) {
        expect_unmapped(table, reg);
    }
}

void test_reset_clears_current_and_pending_state() {
    RenameTable table;
    issue_and_latch(table, 18U, make_tag(13U, 4U));

    table.evaluate_updates(
        make_issue(19U, make_tag(14U, 4U)),
        RATCommit{},
        false
    );

    table.reset();
    table.latch();

    expect_unmapped(table, RegisterIndex{18U});
    expect_unmapped(table, RegisterIndex{19U});
}

}  // namespace

int main(int argc, char* argv[]) {
    test::TestLog log(RENAME_TABLE_TEST_LOG_PATH);
    if (!log.is_open()) {
        std::cerr << "failed to open rename table test log: "
                  << RENAME_TABLE_TEST_LOG_PATH << '\n';
        return EXIT_FAILURE;
    }
    std::cout << "log: " << RENAME_TABLE_TEST_LOG_PATH << '\n';

    struct TestCase {
        std::string_view name;
        void (*run)();
    };

    const TestCase cases[]{
        {"initialization", test_constructor_initializes_all_registers_unmapped},
        {"issue", test_issue_mapping_is_visible_only_after_latch},
        {"invalid-issue", test_invalid_issue_updates_are_ignored},
        {"commit", test_matching_commit_clears_mapping_after_latch},
        {"stale-commit", test_stale_commit_does_not_clear_younger_mapping},
        {"invalid-commit", test_invalid_commit_is_ignored},
        {"priority", test_issue_wins_over_same_cycle_commit},
        {"preserve", test_updates_preserve_unrelated_mappings},
        {"flush", test_flush_has_highest_priority_and_clears_all_mappings},
        {"reset", test_reset_clears_current_and_pending_state},
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
        std::cerr << "unknown rename table test case: " << filter << '\n';
        std::cerr << "available cases:";
        for (const TestCase& test_case : cases) {
            std::cerr << ' ' << test_case.name;
        }
        std::cerr << '\n';
        return EXIT_FAILURE;
    }

    if (test::failure_count != 0) {
        std::cerr << test::failure_count
                  << " rename table test(s) failed\n";
        return EXIT_FAILURE;
    }

    std::cout << "all rename table unit tests passed\n";
    return EXIT_SUCCESS;
}
