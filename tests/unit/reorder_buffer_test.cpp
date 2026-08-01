#include <cstdlib>
#include <iostream>
#include <string_view>

#include "test_log.hpp"
#include "test_support.hpp"
#include "tomasulo/reorder_buffer.hpp"

#ifndef ROB_TEST_LOG_PATH
#define ROB_TEST_LOG_PATH "riscv_rob_unit_tests.log"
#endif

namespace {

void expect_tag(RobTag actual, RobTag expected) {
    EXPECT_EQ(actual.index, expected.index);
    EXPECT_EQ(actual.generation, expected.generation);
    EXPECT_EQ(actual.valid, expected.valid);
}

ROBEntry make_entry(
    RobTag tag,
    OP op = OP::ADD,
    RegisterIndex rd = 5U
) {
    ROBEntry entry{};
    entry.tag = tag;
    entry.op = op;
    entry.pc = 0x1000U + tag.index * 4U;
    entry.rd = rd;
    entry.writes_rd = rd != RegisterIndex{0U};
    entry.predicted_next_pc = entry.pc + 4U;
    entry.valid = true;
    return entry;
}

ROBInputs issue_input(const ROBEntry& entry) {
    ROBInputs inputs{};
    inputs.issue_entry = entry;
    inputs.issue_valid = true;
    return inputs;
}

ROBOutputs issue_and_latch(
    ReorderBuffer& rob,
    ROBEntry entry
) {
    const ROBOutputs outputs = rob.evaluate(issue_input(entry));
    EXPECT_TRUE(outputs.issue_accepted);
    rob.latch();
    return outputs;
}

ROBEntry allocate_entry(
    ReorderBuffer& rob,
    OP op = OP::ADD,
    RegisterIndex rd = 5U
) {
    const RobTag tag = rob.next_tag();
    ROBEntry entry = make_entry(tag, op, rd);
    issue_and_latch(rob, entry);
    return entry;
}

FUResult completion_for(
    RobTag tag,
    Word value,
    bool valid = true
) {
    FUResult result{};
    result.tag = tag;
    result.result = value;
    result.valid = valid;
    return result;
}

void complete_and_latch(
    ReorderBuffer& rob,
    const FUResult& completion
) {
    ROBInputs inputs{};
    inputs.completion = completion;
    const ROBOutputs outputs = rob.evaluate(inputs);
    EXPECT_FALSE(outputs.commit.valid);
    rob.latch();
}

void expect_entry_matches(
    const ROBEntry& actual,
    const ROBEntry& expected
) {
    expect_tag(actual.tag, expected.tag);
    EXPECT_EQ(actual.op, expected.op);
    EXPECT_EQ(actual.pc, expected.pc);
    EXPECT_EQ(actual.rd, expected.rd);
    EXPECT_EQ(actual.writes_rd, expected.writes_rd);
    EXPECT_EQ(actual.valid, expected.valid);
}

void test_constructor_starts_empty() {
    ReorderBuffer rob;

    EXPECT_TRUE(rob.empty());
    EXPECT_FALSE(rob.full());
    EXPECT_EQ(rob.size(), std::size_t{0U});
    EXPECT_TRUE(rob.front() == nullptr);

    const RobTag tag = rob.next_tag();
    EXPECT_TRUE(tag.valid);
    EXPECT_EQ(tag.index, RobIndex{0U});
    EXPECT_EQ(tag.generation, TagGeneration{1U});

    for (const ROBEntry& entry : rob.entries()) {
        EXPECT_FALSE(entry.valid);
    }
}

void test_issue_is_visible_only_after_latch() {
    ReorderBuffer rob;
    const RobTag tag = rob.next_tag();
    const ROBEntry entry = make_entry(tag, OP::SUB, 7U);

    const ROBOutputs outputs = rob.evaluate(issue_input(entry));

    EXPECT_TRUE(outputs.issue_accepted);
    EXPECT_TRUE(rob.empty());
    EXPECT_TRUE(rob.front() == nullptr);
    EXPECT_TRUE(rob.lookup(tag) == nullptr);

    rob.latch();

    EXPECT_FALSE(rob.empty());
    EXPECT_EQ(rob.size(), std::size_t{1U});
    EXPECT_TRUE(rob.front() != nullptr);
    EXPECT_TRUE(rob.lookup(tag) != nullptr);
    if (rob.front() != nullptr) {
        expect_entry_matches(*rob.front(), entry);
    }
}

void test_invalid_issue_requests_are_rejected() {
    ReorderBuffer rob;
    const RobTag expected = rob.next_tag();

    ROBInputs disabled = issue_input(make_entry(expected));
    disabled.issue_valid = false;
    EXPECT_FALSE(rob.evaluate(disabled).issue_accepted);
    rob.latch();
    EXPECT_TRUE(rob.empty());

    ROBEntry invalid_entry = make_entry(expected);
    invalid_entry.valid = false;
    EXPECT_FALSE(
        rob.evaluate(issue_input(invalid_entry)).issue_accepted
    );
    rob.latch();
    EXPECT_TRUE(rob.empty());

    RobTag wrong_tag = expected;
    ++wrong_tag.generation;
    EXPECT_FALSE(
        rob.evaluate(issue_input(make_entry(wrong_tag))).issue_accepted
    );
    rob.latch();
    EXPECT_TRUE(rob.empty());
}

void test_completion_is_visible_and_committed_next_cycle() {
    ReorderBuffer rob;
    const ROBEntry issued = allocate_entry(rob, OP::ADD, 9U);

    const ROBEntry* before_completion = rob.lookup(issued.tag);
    EXPECT_TRUE(before_completion != nullptr);
    if (before_completion != nullptr) {
        EXPECT_FALSE(before_completion->ready);
    }

    complete_and_latch(
        rob,
        completion_for(issued.tag, 0x12345678U)
    );

    const ROBEntry* completed = rob.lookup(issued.tag);
    EXPECT_TRUE(completed != nullptr);
    if (completed != nullptr) {
        EXPECT_TRUE(completed->ready);
        EXPECT_EQ(completed->value, Word{0x12345678U});
    }

    const ROBOutputs commit = rob.evaluate(ROBInputs{});
    EXPECT_TRUE(commit.commit.valid);
    EXPECT_EQ(commit.commit.entry.value, Word{0x12345678U});
    expect_tag(commit.commit.entry.tag, issued.tag);
    EXPECT_FALSE(commit.flush);

    EXPECT_FALSE(rob.empty());
    rob.latch();
    EXPECT_TRUE(rob.empty());
}

void test_stale_and_invalid_completions_are_ignored() {
    ReorderBuffer rob;
    const ROBEntry issued = allocate_entry(rob);

    RobTag stale = issued.tag;
    ++stale.generation;
    complete_and_latch(rob, completion_for(stale, 99U));

    const ROBEntry* after_stale = rob.lookup(issued.tag);
    EXPECT_TRUE(after_stale != nullptr);
    if (after_stale != nullptr) {
        EXPECT_FALSE(after_stale->ready);
        EXPECT_EQ(after_stale->value, Word{0U});
    }

    complete_and_latch(
        rob,
        completion_for(issued.tag, 88U, false)
    );
    const ROBEntry* after_invalid = rob.lookup(issued.tag);
    EXPECT_TRUE(after_invalid != nullptr);
    if (after_invalid != nullptr) {
        EXPECT_FALSE(after_invalid->ready);
    }
}

void test_out_of_order_completion_commits_in_order() {
    ReorderBuffer rob;
    const ROBEntry older = allocate_entry(rob, OP::ADD, 1U);
    const ROBEntry younger = allocate_entry(rob, OP::SUB, 2U);

    complete_and_latch(rob, completion_for(younger.tag, 22U));

    const ROBOutputs blocked = rob.evaluate(ROBInputs{});
    EXPECT_FALSE(blocked.commit.valid);
    rob.latch();

    complete_and_latch(rob, completion_for(older.tag, 11U));

    const ROBOutputs first_commit = rob.evaluate(ROBInputs{});
    EXPECT_TRUE(first_commit.commit.valid);
    expect_tag(first_commit.commit.entry.tag, older.tag);
    rob.latch();

    const ROBOutputs second_commit = rob.evaluate(ROBInputs{});
    EXPECT_TRUE(second_commit.commit.valid);
    expect_tag(second_commit.commit.entry.tag, younger.tag);
    rob.latch();

    EXPECT_TRUE(rob.empty());
}

void test_commit_and_issue_can_share_a_cycle() {
    ReorderBuffer rob;
    ROBEntry departing = make_entry(rob.next_tag(), OP::HALT, 0U);
    departing.ready = true;
    issue_and_latch(rob, departing);

    const ROBEntry arriving = make_entry(rob.next_tag(), OP::ADD, 3U);
    const ROBOutputs outputs = rob.evaluate(issue_input(arriving));

    EXPECT_TRUE(outputs.commit.valid);
    EXPECT_TRUE(outputs.commit.halted);
    EXPECT_TRUE(outputs.issue_accepted);

    rob.latch();

    EXPECT_EQ(rob.size(), std::size_t{1U});
    EXPECT_TRUE(rob.front() != nullptr);
    if (rob.front() != nullptr) {
        expect_tag(rob.front()->tag, arriving.tag);
    }
}

void test_capacity_and_backpressure() {
    ReorderBuffer rob;

    for (std::size_t index = 0U; index < ReorderBuffer::kCapacity; ++index) {
        allocate_entry(
            rob,
            OP::ADD,
            static_cast<RegisterIndex>((index % 31U) + 1U)
        );
    }

    EXPECT_TRUE(rob.full());
    EXPECT_FALSE(rob.empty());
    EXPECT_EQ(rob.size(), ReorderBuffer::kCapacity);
    EXPECT_FALSE(rob.next_tag().valid);

    ROBEntry extra = make_entry(RobTag{0U, 99U, true});
    const ROBOutputs rejected = rob.evaluate(issue_input(extra));
    rob.latch();

    EXPECT_FALSE(rejected.issue_accepted);
    EXPECT_TRUE(rob.full());
    EXPECT_EQ(rob.size(), ReorderBuffer::kCapacity);
}

void test_generation_changes_when_slot_is_reused() {
    ReorderBuffer rob;
    RobTag first_tag{};

    for (std::size_t index = 0U; index < ReorderBuffer::kCapacity; ++index) {
        ROBEntry entry = make_entry(rob.next_tag(), OP::HALT, 0U);
        entry.ready = true;
        if (index == 0U) {
            first_tag = entry.tag;
        }
        issue_and_latch(rob, entry);

        const ROBOutputs commit = rob.evaluate(ROBInputs{});
        EXPECT_TRUE(commit.commit.valid);
        rob.latch();
    }

    EXPECT_TRUE(rob.empty());
    const RobTag reused = rob.next_tag();
    EXPECT_EQ(reused.index, first_tag.index);
    EXPECT_EQ(
        reused.generation,
        first_tag.generation + TagGeneration{1U}
    );
    EXPECT_TRUE(rob.lookup(first_tag) == nullptr);
}

void test_store_waits_for_memory_completion() {
    ReorderBuffer rob;
    ROBEntry store = make_entry(rob.next_tag(), OP::SW, 0U);
    store.is_store = true;
    store.writes_rd = false;
    issue_and_latch(rob, store);

    const ROBOutputs request = rob.evaluate(ROBInputs{});
    EXPECT_TRUE(request.store_request.valid);
    expect_tag(request.store_request.tag, store.tag);
    EXPECT_FALSE(request.commit.valid);
    rob.latch();

    ROBInputs completion{};
    completion.store_completion =
        StoreCommitCompletion{store.tag, true};
    const ROBOutputs completion_outputs = rob.evaluate(completion);
    EXPECT_TRUE(completion_outputs.store_request.valid);
    EXPECT_FALSE(completion_outputs.commit.valid);
    rob.latch();

    const ROBOutputs commit = rob.evaluate(ROBInputs{});
    EXPECT_TRUE(commit.commit.valid);
    EXPECT_TRUE(commit.commit.entry.is_store);
    rob.latch();
    EXPECT_TRUE(rob.empty());
}

void test_non_store_ignores_store_completion() {
    ReorderBuffer rob;
    const ROBEntry entry = allocate_entry(rob, OP::ADD, 4U);

    ROBInputs inputs{};
    inputs.store_completion =
        StoreCommitCompletion{entry.tag, true};
    EXPECT_FALSE(rob.evaluate(inputs).commit.valid);
    rob.latch();

    const ROBEntry* unchanged = rob.lookup(entry.tag);
    EXPECT_TRUE(unchanged != nullptr);
    if (unchanged != nullptr) {
        EXPECT_FALSE(unchanged->ready);
    }
}

void test_control_completion_and_mispredict_flush() {
    ReorderBuffer rob;
    ROBEntry branch = make_entry(rob.next_tag(), OP::BEQ, 0U);
    branch.is_branch = true;
    branch.writes_rd = false;
    branch.predicted_next_pc = branch.pc + 4U;
    issue_and_latch(rob, branch);

    const ROBEntry younger = allocate_entry(rob, OP::ADD, 6U);

    FUResult result = completion_for(branch.tag, 0U);
    result.has_control_result = true;
    result.branch_taken = true;
    result.actual_next_pc = branch.pc + 0x20U;
    complete_and_latch(rob, result);

    const ROBEntry* completed = rob.lookup(branch.tag);
    EXPECT_TRUE(completed != nullptr);
    if (completed != nullptr) {
        EXPECT_TRUE(completed->branch_taken);
        EXPECT_EQ(completed->actual_next_pc, result.actual_next_pc);
    }

    ROBEntry same_cycle_issue = make_entry(rob.next_tag(), OP::SUB, 7U);
    const ROBOutputs commit = rob.evaluate(issue_input(same_cycle_issue));

    EXPECT_TRUE(commit.commit.valid);
    EXPECT_TRUE(commit.commit.mispredicted);
    EXPECT_EQ(commit.commit.redirect_pc, result.actual_next_pc);
    EXPECT_TRUE(commit.flush);
    EXPECT_EQ(commit.redirect_pc, result.actual_next_pc);
    EXPECT_FALSE(commit.issue_accepted);

    rob.latch();

    EXPECT_TRUE(rob.empty());
    EXPECT_TRUE(rob.lookup(younger.tag) == nullptr);
}

void test_correct_control_prediction_does_not_flush() {
    ReorderBuffer rob;
    ROBEntry jump = make_entry(rob.next_tag(), OP::JAL, 1U);
    jump.is_jump = true;
    jump.ready = true;
    jump.actual_next_pc = jump.pc + 0x40U;
    jump.predicted_next_pc = jump.actual_next_pc;
    issue_and_latch(rob, jump);

    const ROBEntry arriving = make_entry(rob.next_tag(), OP::ADD, 8U);
    const ROBOutputs outputs = rob.evaluate(issue_input(arriving));

    EXPECT_TRUE(outputs.commit.valid);
    EXPECT_FALSE(outputs.commit.mispredicted);
    EXPECT_FALSE(outputs.flush);
    EXPECT_TRUE(outputs.issue_accepted);

    rob.latch();
    EXPECT_EQ(rob.size(), std::size_t{1U});
}

void test_reset_clears_current_and_pending_state() {
    ReorderBuffer rob;
    allocate_entry(rob);

    ROBEntry pending = make_entry(rob.next_tag(), OP::SUB, 10U);
    rob.evaluate(issue_input(pending));

    rob.reset();
    rob.latch();

    EXPECT_TRUE(rob.empty());
    EXPECT_FALSE(rob.full());
    EXPECT_EQ(rob.size(), std::size_t{0U});
    EXPECT_TRUE(rob.front() == nullptr);
    EXPECT_EQ(rob.next_tag().index, RobIndex{0U});
    EXPECT_EQ(rob.next_tag().generation, TagGeneration{1U});
}

}  // namespace

int main(int argc, char* argv[]) {
    test::TestLog log(ROB_TEST_LOG_PATH);
    if (!log.is_open()) {
        std::cerr << "failed to open ROB test log: "
                  << ROB_TEST_LOG_PATH << '\n';
        return EXIT_FAILURE;
    }
    std::cout << "log: " << ROB_TEST_LOG_PATH << '\n';

    struct TestCase {
        std::string_view name;
        void (*run)();
    };

    const TestCase cases[]{
        {"initialization", test_constructor_starts_empty},
        {"issue", test_issue_is_visible_only_after_latch},
        {"invalid-issue", test_invalid_issue_requests_are_rejected},
        {"completion", test_completion_is_visible_and_committed_next_cycle},
        {"stale-completion", test_stale_and_invalid_completions_are_ignored},
        {"ordered-commit", test_out_of_order_completion_commits_in_order},
        {"simultaneous", test_commit_and_issue_can_share_a_cycle},
        {"capacity", test_capacity_and_backpressure},
        {"generation", test_generation_changes_when_slot_is_reused},
        {"store", test_store_waits_for_memory_completion},
        {"non-store", test_non_store_ignores_store_completion},
        {"mispredict", test_control_completion_and_mispredict_flush},
        {"correct-control", test_correct_control_prediction_does_not_flush},
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
        std::cerr << "unknown ROB test case: " << filter << '\n';
        std::cerr << "available cases:";
        for (const TestCase& test_case : cases) {
            std::cerr << ' ' << test_case.name;
        }
        std::cerr << '\n';
        return EXIT_FAILURE;
    }

    if (test::failure_count != 0) {
        std::cerr << test::failure_count << " ROB test(s) failed\n";
        return EXIT_FAILURE;
    }

    std::cout << "all ROB unit tests passed\n";
    return EXIT_SUCCESS;
}
