#include <cstdlib>
#include <iostream>
#include <string_view>

#include "test_log.hpp"
#include "test_support.hpp"
#include "tomasulo/reservation_station.hpp"

namespace {

constexpr RobTag make_tag(
    RobIndex index,
    TagGeneration generation = 1U
) {
    return RobTag{index, generation, true};
}

Operand make_ready_operand(Word value) {
    Operand operand{};
    operand.value = value;
    operand.ready = true;
    return operand;
}

Operand make_waiting_operand(RobTag tag) {
    Operand operand{};
    operand.tag = tag;
    operand.ready = false;
    return operand;
}

RSEntry make_entry(
    RobTag destination,
    Operand lhs = make_ready_operand(10U),
    Operand rhs = make_ready_operand(20U),
    OP op = OP::ADD
) {
    RSEntry entry{};
    entry.op = op;
    entry.destination = destination;
    entry.lhs = lhs;
    entry.rhs = rhs;
    entry.immediate = 0x44U;
    entry.pc = 0x1000U;
    entry.predicted_next_pc = 0x1004U;
    return entry;
}

RSInputs issue_input(const RSEntry& entry) {
    RSInputs inputs{};
    inputs.issue_entry = entry;
    inputs.issue_valid = true;
    return inputs;
}

void expect_tag(RobTag actual, RobTag expected) {
    EXPECT_EQ(actual.index, expected.index);
    EXPECT_EQ(actual.generation, expected.generation);
    EXPECT_EQ(actual.valid, expected.valid);
}

void expect_execute_matches(
    const Execute& execute,
    const RSEntry& entry
) {
    EXPECT_TRUE(execute.valid);
    EXPECT_EQ(execute.op, entry.op);
    EXPECT_EQ(execute.operand1, entry.lhs.value);
    EXPECT_EQ(execute.operand2, entry.rhs.value);
    EXPECT_EQ(execute.immediate, entry.immediate);
    EXPECT_EQ(execute.pc, entry.pc);
    expect_tag(execute.tag, entry.destination);
}

void insert_entry(
    ReservationStation& station,
    const RSEntry& entry
) {
    const RSOutputs outputs = station.evaluate(issue_input(entry));
    EXPECT_TRUE(outputs.issue_accepted);
    station.latch();
}

void test_constructor_starts_empty() {
    ReservationStation station;

    EXPECT_TRUE(station.empty());
    EXPECT_FALSE(station.full());
    EXPECT_TRUE(station.available());
    EXPECT_EQ(station.size(), std::size_t{0U});
}

void test_issue_is_visible_only_after_latch() {
    ReservationStation station;
    const RSEntry entry = make_entry(make_tag(1U, 2U));

    const RSOutputs outputs = station.evaluate(issue_input(entry));

    EXPECT_TRUE(outputs.issue_accepted);
    EXPECT_FALSE(outputs.dispatch_valid);
    EXPECT_TRUE(station.empty());
    EXPECT_EQ(station.size(), std::size_t{0U});

    station.latch();

    EXPECT_FALSE(station.empty());
    EXPECT_EQ(station.size(), std::size_t{1U});
    EXPECT_TRUE(station.available());
}

void test_invalid_issue_does_not_allocate_slot() {
    ReservationStation station;
    RSInputs inputs{};
    inputs.issue_entry = make_entry(make_tag(2U));
    inputs.issue_valid = false;

    const RSOutputs outputs = station.evaluate(inputs);
    station.latch();

    EXPECT_FALSE(outputs.issue_accepted);
    EXPECT_TRUE(station.empty());
    EXPECT_EQ(station.size(), std::size_t{0U});
}

void test_capacity_and_backpressure() {
    ReservationStation station;

    for (std::size_t index = 0U; index < kCapacity; ++index) {
        insert_entry(
            station,
            make_entry(make_tag(static_cast<RobIndex>(index)))
        );
    }

    EXPECT_FALSE(station.empty());
    EXPECT_TRUE(station.full());
    EXPECT_FALSE(station.available());
    EXPECT_EQ(station.size(), kCapacity);

    const RSOutputs rejected = station.evaluate(
        issue_input(make_entry(make_tag(99U)))
    );
    station.latch();

    EXPECT_FALSE(rejected.issue_accepted);
    EXPECT_TRUE(station.full());
    EXPECT_EQ(station.size(), kCapacity);
}

void test_ready_entry_uses_fu_valid_ready_handshake() {
    ReservationStation station;
    const RSEntry entry = make_entry(
        make_tag(3U, 4U),
        make_ready_operand(0x11111111U),
        make_ready_operand(0x22222222U),
        OP::SUB
    );
    insert_entry(station, entry);

    RSInputs stalled{};
    stalled.fu_available = false;
    const RSOutputs stalled_outputs = station.evaluate(stalled);

    EXPECT_TRUE(stalled_outputs.dispatch_valid);
    expect_execute_matches(stalled_outputs.dispatch, entry);

    station.latch();
    EXPECT_EQ(station.size(), std::size_t{1U});

    RSInputs accepted{};
    accepted.fu_available = true;
    const RSOutputs accepted_outputs = station.evaluate(accepted);

    EXPECT_TRUE(accepted_outputs.dispatch_valid);
    expect_execute_matches(accepted_outputs.dispatch, entry);

    station.latch();
    EXPECT_TRUE(station.empty());
}

void test_unready_entry_is_not_dispatched() {
    ReservationStation station;
    insert_entry(
        station,
        make_entry(
            make_tag(4U),
            make_waiting_operand(make_tag(20U)),
            make_ready_operand(8U)
        )
    );

    RSInputs inputs{};
    inputs.fu_available = true;
    const RSOutputs outputs = station.evaluate(inputs);
    station.latch();

    EXPECT_FALSE(outputs.dispatch_valid);
    EXPECT_FALSE(outputs.dispatch.valid);
    EXPECT_EQ(station.size(), std::size_t{1U});
}

void test_cdb_wakeup_takes_effect_next_cycle() {
    ReservationStation station;
    const RobTag producer = make_tag(21U, 5U);
    const RSEntry entry = make_entry(
        make_tag(5U),
        make_waiting_operand(producer),
        make_ready_operand(9U),
        OP::XOR
    );
    insert_entry(station, entry);

    RSInputs wake{};
    wake.cdb = CDBMsg{producer, 0xaabbccddU, true};
    wake.fu_available = true;
    const RSOutputs wake_outputs = station.evaluate(wake);

    EXPECT_FALSE(wake_outputs.dispatch_valid);

    station.latch();

    RSInputs dispatch{};
    dispatch.fu_available = true;
    const RSOutputs dispatch_outputs = station.evaluate(dispatch);

    EXPECT_TRUE(dispatch_outputs.dispatch_valid);
    RSEntry expected = entry;
    expected.lhs = make_ready_operand(0xaabbccddU);
    expect_execute_matches(dispatch_outputs.dispatch, expected);

    station.latch();
    EXPECT_TRUE(station.empty());
}

void test_cdb_requires_full_tag_match() {
    ReservationStation station;
    const RobTag producer = make_tag(22U, 8U);
    insert_entry(
        station,
        make_entry(
            make_tag(6U),
            make_waiting_operand(producer),
            make_ready_operand(1U)
        )
    );

    RSInputs stale_broadcast{};
    stale_broadcast.cdb =
        CDBMsg{make_tag(22U, 7U), 0xdeadbeefU, true};
    stale_broadcast.fu_available = true;
    const RSOutputs stale_outputs =
        station.evaluate(stale_broadcast);
    station.latch();

    EXPECT_FALSE(stale_outputs.dispatch_valid);

    RSInputs next_cycle{};
    next_cycle.fu_available = true;
    const RSOutputs next_outputs = station.evaluate(next_cycle);
    station.latch();

    EXPECT_FALSE(next_outputs.dispatch_valid);
    EXPECT_EQ(station.size(), std::size_t{1U});
}

void test_dispatch_and_issue_can_share_a_cycle() {
    ReservationStation station;
    const RSEntry departing = make_entry(make_tag(7U));
    const RSEntry arriving = make_entry(
        make_tag(8U),
        make_waiting_operand(make_tag(30U)),
        make_ready_operand(2U)
    );
    insert_entry(station, departing);

    RSInputs inputs = issue_input(arriving);
    inputs.fu_available = true;
    const RSOutputs outputs = station.evaluate(inputs);

    EXPECT_TRUE(outputs.dispatch_valid);
    expect_execute_matches(outputs.dispatch, departing);
    EXPECT_TRUE(outputs.issue_accepted);

    station.latch();

    EXPECT_EQ(station.size(), std::size_t{1U});

    RSInputs no_dispatch{};
    no_dispatch.fu_available = true;
    const RSOutputs next_outputs = station.evaluate(no_dispatch);
    station.latch();

    EXPECT_FALSE(next_outputs.dispatch_valid);
    EXPECT_EQ(station.size(), std::size_t{1U});
}

void test_round_robin_does_not_immediately_reuse_low_slot() {
    ReservationStation station;
    const RSEntry first = make_entry(make_tag(40U));
    const RSEntry second = make_entry(make_tag(41U));
    insert_entry(station, first);
    insert_entry(station, second);

    RSInputs dispatch_first{};
    dispatch_first.fu_available = true;
    const RSOutputs first_outputs = station.evaluate(dispatch_first);
    expect_execute_matches(first_outputs.dispatch, first);
    station.latch();

    const RSEntry reused_low_slot = make_entry(make_tag(42U));
    insert_entry(station, reused_low_slot);

    RSInputs dispatch_second{};
    dispatch_second.fu_available = true;
    const RSOutputs second_outputs = station.evaluate(dispatch_second);

    EXPECT_TRUE(second_outputs.dispatch_valid);
    expect_execute_matches(second_outputs.dispatch, second);
    station.latch();
}

void test_flush_suppresses_actions_and_clears_station() {
    ReservationStation station;
    insert_entry(station, make_entry(make_tag(50U)));
    insert_entry(
        station,
        make_entry(
            make_tag(51U),
            make_waiting_operand(make_tag(60U)),
            make_ready_operand(3U)
        )
    );

    RSInputs flush{};
    flush.flush = true;
    flush.fu_available = true;
    flush.issue_valid = true;
    flush.issue_entry = make_entry(make_tag(52U));
    flush.cdb = CDBMsg{make_tag(60U), 99U, true};

    const RSOutputs outputs = station.evaluate(flush);

    EXPECT_FALSE(outputs.dispatch_valid);
    EXPECT_FALSE(outputs.issue_accepted);
    EXPECT_EQ(station.size(), std::size_t{2U});

    station.latch();

    EXPECT_TRUE(station.empty());
    EXPECT_EQ(station.size(), std::size_t{0U});
}

void test_reset_clears_current_and_pending_state() {
    ReservationStation station;
    insert_entry(station, make_entry(make_tag(70U)));

    station.evaluate(issue_input(make_entry(make_tag(71U))));
    station.reset();
    station.latch();

    EXPECT_TRUE(station.empty());
    EXPECT_EQ(station.size(), std::size_t{0U});
    EXPECT_TRUE(station.available());
}

}  // namespace

int main(int argc, char* argv[]) {
    test::TestLog log(RESERVATION_STATION_TEST_LOG_PATH);
    if (!log.is_open()) {
        std::cerr << "failed to open reservation station test log: "
                  << RESERVATION_STATION_TEST_LOG_PATH << '\n';
        return EXIT_FAILURE;
    }
    std::cout << "log: " << RESERVATION_STATION_TEST_LOG_PATH << '\n';

    struct TestCase {
        std::string_view name;
        void (*run)();
    };

    const TestCase cases[]{
        {"initialization", test_constructor_starts_empty},
        {"issue", test_issue_is_visible_only_after_latch},
        {"invalid-issue", test_invalid_issue_does_not_allocate_slot},
        {"capacity", test_capacity_and_backpressure},
        {"dispatch", test_ready_entry_uses_fu_valid_ready_handshake},
        {"unready", test_unready_entry_is_not_dispatched},
        {"cdb", test_cdb_wakeup_takes_effect_next_cycle},
        {"tag-generation", test_cdb_requires_full_tag_match},
        {"simultaneous", test_dispatch_and_issue_can_share_a_cycle},
        {"round-robin", test_round_robin_does_not_immediately_reuse_low_slot},
        {"flush", test_flush_suppresses_actions_and_clears_station},
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
        std::cerr << "unknown reservation station test case: "
                  << filter << '\n';
        std::cerr << "available cases:";
        for (const TestCase& test_case : cases) {
            std::cerr << ' ' << test_case.name;
        }
        std::cerr << '\n';
        return EXIT_FAILURE;
    }

    if (test::failure_count != 0) {
        std::cerr << test::failure_count
                  << " reservation station test(s) failed\n";
        return EXIT_FAILURE;
    }

    std::cout << "all reservation station unit tests passed\n";
    return EXIT_SUCCESS;
}
