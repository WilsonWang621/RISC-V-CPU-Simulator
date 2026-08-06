#include <array>
#include <cstdlib>
#include <iostream>
#include <string_view>

#include "test_log.hpp"
#include "test_support.hpp"
#include "tomasulo/load_store_queue.hpp"

#ifndef LSQ_TEST_LOG_PATH
#define LSQ_TEST_LOG_PATH "riscv_lsq_unit_tests.log"
#endif

namespace {

constexpr RobTag make_tag(
    RobIndex index,
    TagGeneration generation = 1U
) {
    return RobTag{index, generation, true};
}

Operand ready_operand(Word value) {
    Operand operand{};
    operand.value = value;
    operand.ready = true;
    return operand;
}

Operand waiting_operand(RobTag tag) {
    Operand operand{};
    operand.tag = tag;
    operand.ready = false;
    return operand;
}

LSQEntry make_load(
    RobTag destination,
    OP op = OP::LW,
    Operand base = ready_operand(0x1000U),
    Word immediate = 0x20U
) {
    LSQEntry entry{};
    entry.type = MemoryAccessType::LOAD;
    entry.op = op;
    entry.destination = destination;
    entry.base = base;
    entry.immediate = immediate;
    entry.valid = true;
    return entry;
}

LSQEntry make_store(
    RobTag destination,
    OP op = OP::SW,
    Operand base = ready_operand(0x2000U),
    Operand data = ready_operand(0x12345678U),
    Word immediate = 0x10U
) {
    LSQEntry entry{};
    entry.type = MemoryAccessType::STORE;
    entry.op = op;
    entry.destination = destination;
    entry.base = base;
    entry.store_data = data;
    entry.immediate = immediate;
    entry.valid = true;
    return entry;
}

LSQInputs issue_input(const LSQEntry& entry) {
    LSQInputs inputs{};
    inputs.issue_entry = entry;
    inputs.issue_valid = true;
    return inputs;
}

DataMemoryResponse memory_response(
    RobTag tag,
    Word value,
    bool is_store = false,
    bool success = true
) {
    DataMemoryResponse response{};
    response.tag = tag;
    response.value = value;
    response.is_store = is_store;
    response.success = success;
    response.valid = true;
    return response;
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
}

void expect_no_actions(const LSQOutputs& outputs) {
    EXPECT_FALSE(outputs.memory_request.valid);
    EXPECT_FALSE(outputs.store_completion.valid);
    EXPECT_FALSE(outputs.issue_accepted);
    EXPECT_FALSE(outputs.memory_error);
}

LSQOutputs plan_and_apply(LoadStoreQueue& queue, const LSQInputs& inputs) {
    LSQObserveInputs observe{};
    observe.store_request = inputs.store_request;
    observe.memory_response = inputs.memory_response;
    observe.memory_available = inputs.memory_available;
    observe.load_result_granted = inputs.load_result_granted;
    observe.flush = inputs.flush;

    const LSQDecision decision = queue.plan(observe);
    LSQOutputs outputs = decision.outputs;
    outputs.issue_accepted = queue.apply(inputs, decision);
    return outputs;
}

void insert_and_latch(LoadStoreQueue& queue, const LSQEntry& entry) {
    const LSQOutputs outputs = plan_and_apply(queue, issue_input(entry));
    EXPECT_TRUE(outputs.issue_accepted);
    queue.latch();
}

void test_constructor_starts_empty() {
    LoadStoreQueue queue;

    EXPECT_TRUE(queue.empty());
    EXPECT_FALSE(queue.full());
    EXPECT_EQ(queue.size(), std::size_t{0});
    expect_empty_result(queue.load_result());

    for (const LSQEntry& entry : queue.entries()) {
        EXPECT_FALSE(entry.valid);
    }
}

void test_issue_is_visible_only_after_latch() {
    LoadStoreQueue queue;
    const RobTag destination = make_tag(1U, 3U);
    const LSQEntry load = make_load(
        destination,
        OP::LW,
        ready_operand(0x1000U),
        0x24U
    );

    const LSQOutputs outputs = plan_and_apply(queue, issue_input(load));
    EXPECT_TRUE(outputs.issue_accepted);
    EXPECT_TRUE(queue.empty());
    EXPECT_EQ(queue.size(), std::size_t{0});

    queue.latch();

    EXPECT_FALSE(queue.empty());
    EXPECT_EQ(queue.size(), std::size_t{1});
    const LSQEntry& stored = queue.entries()[0];
    EXPECT_TRUE(stored.valid);
    EXPECT_EQ(stored.type, MemoryAccessType::LOAD);
    EXPECT_EQ(stored.op, OP::LW);
    expect_tag(stored.destination, destination);
    EXPECT_TRUE(stored.address_ready);
    EXPECT_EQ(stored.address, Address{0x1024U});
    EXPECT_FALSE(stored.request_sent);
}

void test_invalid_issue_does_not_allocate() {
    LoadStoreQueue queue;

    LSQInputs invalid_signal{};
    invalid_signal.issue_entry = make_load(make_tag(2U));
    expect_no_actions(plan_and_apply(queue, invalid_signal));
    queue.latch();
    EXPECT_TRUE(queue.empty());

    LSQInputs invalid_entry{};
    invalid_entry.issue_valid = true;
    expect_no_actions(plan_and_apply(queue, invalid_entry));
    queue.latch();
    EXPECT_TRUE(queue.empty());
}

void test_capacity_and_backpressure() {
    LoadStoreQueue queue;

    for (std::size_t index = 0; index < LoadStoreQueue::kCapacity; ++index) {
        const LSQOutputs outputs = plan_and_apply(queue,
            issue_input(make_load(make_tag(static_cast<RobIndex>(index + 1U))))
        );
        EXPECT_TRUE(outputs.issue_accepted);
        queue.latch();
    }

    EXPECT_TRUE(queue.full());
    EXPECT_EQ(queue.size(), LoadStoreQueue::kCapacity);

    const LSQOutputs rejected = plan_and_apply(queue,
        issue_input(make_load(make_tag(100U)))
    );
    EXPECT_FALSE(rejected.issue_accepted);
    queue.latch();
    EXPECT_TRUE(queue.full());
    EXPECT_EQ(queue.size(), LoadStoreQueue::kCapacity);
}

void test_cdb_wakes_base_and_store_data_after_latch() {
    LoadStoreQueue queue;
    const RobTag base_tag = make_tag(20U, 2U);
    const RobTag data_tag = make_tag(21U, 4U);
    insert_and_latch(
        queue,
        make_store(
            make_tag(22U),
            OP::SW,
            waiting_operand(base_tag),
            waiting_operand(data_tag),
            8U
        )
    );

    LSQInputs wake_base{};
    wake_base.cdb = CDBMsg{base_tag, 0x3000U, true};
    (void)plan_and_apply(queue, wake_base);
    EXPECT_FALSE(queue.entries()[0].base.ready);
    queue.latch();

    EXPECT_TRUE(queue.entries()[0].base.ready);
    EXPECT_EQ(queue.entries()[0].base.value, Word{0x3000U});
    EXPECT_TRUE(queue.entries()[0].address_ready);
    EXPECT_EQ(queue.entries()[0].address, Address{0x3008U});
    EXPECT_FALSE(queue.entries()[0].store_data.ready);

    LSQInputs wake_data{};
    wake_data.cdb = CDBMsg{data_tag, 0xdeadbeefU, true};
    (void)plan_and_apply(queue, wake_data);
    queue.latch();

    EXPECT_TRUE(queue.entries()[0].store_data.ready);
    EXPECT_EQ(queue.entries()[0].store_data.value, Word{0xdeadbeefU});
}

void test_load_request_contains_expected_fields() {
    LoadStoreQueue queue;
    const RobTag destination = make_tag(30U, 5U);
    insert_and_latch(
        queue,
        make_load(destination, OP::LH, ready_operand(0x4000U), 6U)
    );

    LSQInputs inputs{};
    inputs.memory_available = true;
    const LSQOutputs outputs = plan_and_apply(queue, inputs);

    EXPECT_TRUE(outputs.memory_request.valid);
    EXPECT_EQ(outputs.memory_request.type, MemoryAccessType::LOAD);
    EXPECT_EQ(outputs.memory_request.width, MemoryWidth::HALF);
    EXPECT_EQ(outputs.memory_request.address, Address{0x4006U});
    EXPECT_EQ(outputs.memory_request.value, Word{0});
    expect_tag(outputs.memory_request.tag, destination);

    queue.latch();
    EXPECT_TRUE(queue.entries()[0].request_sent);

    const LSQOutputs duplicate = plan_and_apply(queue, inputs);
    EXPECT_FALSE(duplicate.memory_request.valid);
}

void test_store_requires_matching_commit_authorization() {
    LoadStoreQueue queue;
    const RobTag destination = make_tag(40U, 8U);
    insert_and_latch(
        queue,
        make_store(
            destination,
            OP::SB,
            ready_operand(0x5000U),
            ready_operand(0xabcdef42U),
            3U
        )
    );

    LSQInputs unavailable{};
    unavailable.memory_available = true;
    EXPECT_FALSE(plan_and_apply(queue, unavailable).memory_request.valid);

    LSQInputs wrong_tag{};
    wrong_tag.memory_available = true;
    wrong_tag.store_request = StoreCommitRequest{make_tag(41U), true};
    EXPECT_FALSE(plan_and_apply(queue, wrong_tag).memory_request.valid);

    LSQInputs authorized{};
    authorized.memory_available = true;
    authorized.store_request = StoreCommitRequest{destination, true};
    const LSQOutputs outputs = plan_and_apply(queue, authorized);

    EXPECT_TRUE(outputs.memory_request.valid);
    EXPECT_EQ(outputs.memory_request.type, MemoryAccessType::STORE);
    EXPECT_EQ(outputs.memory_request.width, MemoryWidth::BYTE);
    EXPECT_EQ(outputs.memory_request.address, Address{0x5003U});
    EXPECT_EQ(outputs.memory_request.value, Word{0xabcdef42U});
    expect_tag(outputs.memory_request.tag, destination);
}

void test_load_response_is_extended_and_buffered() {
    struct Case {
        OP op;
        Word raw;
        Word expected;
    };

    constexpr std::array<Case, 5> cases{{
        {OP::LB, 0x00000080U, 0xffffff80U},
        {OP::LBU, 0xffffff80U, 0x00000080U},
        {OP::LH, 0x00008001U, 0xffff8001U},
        {OP::LHU, 0xffff8001U, 0x00008001U},
        {OP::LW, 0x89abcdefU, 0x89abcdefU},
    }};

    for (std::size_t index = 0; index < cases.size(); ++index) {
        LoadStoreQueue queue;
        const RobTag destination =
            make_tag(static_cast<RobIndex>(50U + index), 2U);
        insert_and_latch(
            queue,
            make_load(destination, cases[index].op)
        );

        LSQInputs send{};
        send.memory_available = true;
        EXPECT_TRUE(plan_and_apply(queue, send).memory_request.valid);
        queue.latch();

        LSQInputs response{};
        response.memory_response =
            memory_response(destination, cases[index].raw);
        const LSQOutputs response_outputs = plan_and_apply(queue, response);
        EXPECT_FALSE(response_outputs.memory_error);
        expect_empty_result(queue.load_result());
        queue.latch();

        const FUResult result = queue.load_result();
        EXPECT_TRUE(result.valid);
        expect_tag(result.tag, destination);
        EXPECT_EQ(result.result, cases[index].expected);
        EXPECT_TRUE(queue.empty());
    }
}

void test_load_result_waits_for_cdb_grant() {
    LoadStoreQueue queue;
    const RobTag destination = make_tag(60U);
    insert_and_latch(queue, make_load(destination));

    LSQInputs send{};
    send.memory_available = true;
    (void)plan_and_apply(queue, send);
    queue.latch();

    LSQInputs response{};
    response.memory_response = memory_response(destination, 0x13572468U);
    (void)plan_and_apply(queue, response);
    queue.latch();
    EXPECT_TRUE(queue.load_result().valid);

    (void)plan_and_apply(queue, LSQInputs{});
    queue.latch();
    EXPECT_TRUE(queue.load_result().valid);

    LSQInputs granted{};
    granted.load_result_granted = true;
    (void)plan_and_apply(queue, granted);
    EXPECT_TRUE(queue.load_result().valid);
    queue.latch();
    expect_empty_result(queue.load_result());
}

void test_store_response_reports_completion() {
    LoadStoreQueue queue;
    const RobTag destination = make_tag(70U, 6U);
    insert_and_latch(queue, make_store(destination));

    LSQInputs send{};
    send.memory_available = true;
    send.store_request = StoreCommitRequest{destination, true};
    EXPECT_TRUE(plan_and_apply(queue, send).memory_request.valid);
    queue.latch();

    LSQInputs response{};
    response.memory_response =
        memory_response(destination, 0U, true, true);
    const LSQOutputs outputs = plan_and_apply(queue, response);

    EXPECT_TRUE(outputs.store_completion.valid);
    expect_tag(outputs.store_completion.tag, destination);
    EXPECT_FALSE(outputs.memory_error);
    EXPECT_EQ(queue.size(), std::size_t{1});
    queue.latch();
    EXPECT_TRUE(queue.empty());
}

void test_mismatched_and_failed_responses() {
    LoadStoreQueue queue;
    const RobTag destination = make_tag(80U, 3U);
    insert_and_latch(queue, make_load(destination));

    LSQInputs send{};
    send.memory_available = true;
    (void)plan_and_apply(queue, send);
    queue.latch();

    LSQInputs stale{};
    stale.memory_response = memory_response(make_tag(80U, 4U), 99U);
    expect_no_actions(plan_and_apply(queue, stale));
    queue.latch();
    EXPECT_EQ(queue.size(), std::size_t{1});
    expect_empty_result(queue.load_result());

    LSQInputs failed{};
    failed.memory_response =
        memory_response(destination, 0U, false, false);
    const LSQOutputs outputs = plan_and_apply(queue, failed);
    EXPECT_TRUE(outputs.memory_error);
    EXPECT_FALSE(outputs.store_completion.valid);
    queue.latch();
    EXPECT_EQ(queue.size(), std::size_t{1});
    expect_empty_result(queue.load_result());
}

void test_only_head_can_send_memory_request() {
    LoadStoreQueue queue;
    const RobTag first_tag = make_tag(90U);
    const RobTag second_tag = make_tag(91U);

    insert_and_latch(
        queue,
        make_load(first_tag, OP::LW, waiting_operand(make_tag(92U)))
    );
    insert_and_latch(queue, make_load(second_tag));

    LSQInputs inputs{};
    inputs.memory_available = true;
    const LSQOutputs outputs = plan_and_apply(queue, inputs);
    EXPECT_FALSE(outputs.memory_request.valid);
    EXPECT_EQ(queue.size(), std::size_t{2});
}

void test_flush_and_reset_clear_all_state() {
    LoadStoreQueue queue;
    insert_and_latch(queue, make_load(make_tag(100U)));
    insert_and_latch(queue, make_store(make_tag(101U)));
    EXPECT_EQ(queue.size(), std::size_t{2});

    LSQInputs flush{};
    flush.flush = true;
    expect_no_actions(plan_and_apply(queue, flush));
    EXPECT_EQ(queue.size(), std::size_t{2});
    queue.latch();

    EXPECT_TRUE(queue.empty());
    expect_empty_result(queue.load_result());
    for (const LSQEntry& entry : queue.entries()) {
        EXPECT_FALSE(entry.valid);
    }

    (void)plan_and_apply(queue, issue_input(make_load(make_tag(102U))));
    queue.reset();
    queue.latch();
    EXPECT_TRUE(queue.empty());
    expect_empty_result(queue.load_result());
}

}  // namespace

int main(int argc, char* argv[]) {
    test::TestLog log(LSQ_TEST_LOG_PATH);
    if (!log.is_open()) {
        std::cerr << "failed to open LSQ test log: "
                  << LSQ_TEST_LOG_PATH << '\n';
        return EXIT_FAILURE;
    }
    std::cout << "log: " << LSQ_TEST_LOG_PATH << '\n';

    struct TestCase {
        std::string_view name;
        void (*run)();
    };

    const TestCase cases[]{
        {"initialization", test_constructor_starts_empty},
        {"issue", test_issue_is_visible_only_after_latch},
        {"invalid-issue", test_invalid_issue_does_not_allocate},
        {"capacity", test_capacity_and_backpressure},
        {"cdb", test_cdb_wakes_base_and_store_data_after_latch},
        {"load-request", test_load_request_contains_expected_fields},
        {"store-request", test_store_requires_matching_commit_authorization},
        {"load-response", test_load_response_is_extended_and_buffered},
        {"load-grant", test_load_result_waits_for_cdb_grant},
        {"store-response", test_store_response_reports_completion},
        {"bad-response", test_mismatched_and_failed_responses},
        {"head-order", test_only_head_can_send_memory_request},
        {"flush-reset", test_flush_and_reset_clear_all_state},
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
        std::cerr << "unknown LSQ test case: " << filter << '\n';
        std::cerr << "available cases:";
        for (const TestCase& test_case : cases) {
            std::cerr << ' ' << test_case.name;
        }
        std::cerr << '\n';
        return EXIT_FAILURE;
    }

    if (test::failure_count != 0) {
        std::cerr << test::failure_count << " LSQ test(s) failed\n";
        return EXIT_FAILURE;
    }

    std::cout << "all LSQ unit tests passed\n";
    return EXIT_SUCCESS;
}
