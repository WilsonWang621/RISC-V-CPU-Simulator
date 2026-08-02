#include <cstdlib>
#include <iostream>
#include <string_view>

#include "test_log.hpp"
#include "test_support.hpp"
#include "tomasulo/common_data_bus.hpp"

#ifndef CDB_TEST_LOG_PATH
#define CDB_TEST_LOG_PATH "riscv_cdb_unit_tests.log"
#endif

namespace {

constexpr RobTag make_tag(
    RobIndex index,
    TagGeneration generation = 1U,
    bool valid = true
) {
    return RobTag{index, generation, valid};
}

FUResult make_result(
    RobTag tag,
    Word value,
    Address actual_next_pc = 0U,
    bool branch_taken = false,
    bool has_control_result = false,
    bool valid = true
) {
    FUResult result{};
    result.tag = tag;
    result.result = value;
    result.actual_next_pc = actual_next_pc;
    result.branch_taken = branch_taken;
    result.has_control_result = has_control_result;
    result.valid = valid;
    return result;
}

void expect_tag(RobTag actual, RobTag expected) {
    EXPECT_EQ(actual.index, expected.index);
    EXPECT_EQ(actual.generation, expected.generation);
    EXPECT_EQ(actual.valid, expected.valid);
}

void expect_empty(const CDBOutputs& outputs) {
    EXPECT_EQ(outputs.source, CDBSource::None);
    EXPECT_FALSE(outputs.integer_granted);
    EXPECT_FALSE(outputs.load_granted);

    EXPECT_FALSE(outputs.broadcast.valid);
    EXPECT_FALSE(outputs.broadcast.tag.valid);
    EXPECT_EQ(outputs.broadcast.value, Word{0});

    EXPECT_FALSE(outputs.completion.valid);
    EXPECT_FALSE(outputs.completion.tag.valid);
    EXPECT_EQ(outputs.completion.result, Word{0});
}

void expect_selected(
    const CDBOutputs& outputs,
    CDBSource expected_source,
    const FUResult& expected
) {
    EXPECT_EQ(outputs.source, expected_source);
    EXPECT_EQ(
        outputs.integer_granted,
        expected_source == CDBSource::IntegerUnit
    );
    EXPECT_EQ(
        outputs.load_granted,
        expected_source == CDBSource::LoadUnit
    );

    EXPECT_TRUE(outputs.broadcast.valid);
    expect_tag(outputs.broadcast.tag, expected.tag);
    EXPECT_EQ(outputs.broadcast.value, expected.result);

    EXPECT_TRUE(outputs.completion.valid);
    expect_tag(outputs.completion.tag, expected.tag);
    EXPECT_EQ(outputs.completion.result, expected.result);
    EXPECT_EQ(outputs.completion.actual_next_pc, expected.actual_next_pc);
    EXPECT_EQ(outputs.completion.branch_taken, expected.branch_taken);
    EXPECT_EQ(
        outputs.completion.has_control_result,
        expected.has_control_result
    );
}

void test_empty_and_invalid_candidates_are_ignored() {
    CommonDataBus bus;

    expect_empty(bus.evaluate(CDBInputs{}));

    CDBInputs invalid_integer{};
    invalid_integer.integer_result =
        make_result(make_tag(1U), 0x11111111U, 0U, false, false, false);
    expect_empty(bus.evaluate(invalid_integer));

    CDBInputs invalid_load{};
    invalid_load.load_result =
        make_result(make_tag(2U, 1U, false), 0x22222222U);
    expect_empty(bus.evaluate(invalid_load));
}

void test_integer_candidate_is_broadcast() {
    CommonDataBus bus;
    const FUResult integer = make_result(
        make_tag(3U, 7U),
        0x12345678U,
        0x2000U,
        true,
        true
    );

    CDBInputs inputs{};
    inputs.integer_result = integer;

    expect_selected(
        bus.evaluate(inputs),
        CDBSource::IntegerUnit,
        integer
    );
}

void test_load_candidate_is_broadcast() {
    CommonDataBus bus;
    const FUResult load = make_result(make_tag(4U, 9U), 0xaabbccddU);

    CDBInputs inputs{};
    inputs.load_result = load;

    expect_selected(bus.evaluate(inputs), CDBSource::LoadUnit, load);
}

void test_competition_grants_exactly_one_producer() {
    CommonDataBus bus;
    const FUResult integer = make_result(make_tag(5U), 50U);
    const FUResult load = make_result(make_tag(6U), 60U);

    CDBInputs inputs{};
    inputs.integer_result = integer;
    inputs.load_result = load;

    const CDBOutputs outputs = bus.evaluate(inputs);
    expect_selected(outputs, CDBSource::IntegerUnit, integer);
    EXPECT_EQ(
        static_cast<unsigned>(outputs.integer_granted)
            + static_cast<unsigned>(outputs.load_granted),
        1U
    );
}

void test_round_robin_alternates_after_latch() {
    CommonDataBus bus;
    const FUResult integer = make_result(make_tag(7U), 70U);
    const FUResult load = make_result(make_tag(8U), 80U);

    CDBInputs inputs{};
    inputs.integer_result = integer;
    inputs.load_result = load;

    expect_selected(
        bus.evaluate(inputs),
        CDBSource::IntegerUnit,
        integer
    );
    bus.latch();

    expect_selected(bus.evaluate(inputs), CDBSource::LoadUnit, load);
    bus.latch();

    expect_selected(
        bus.evaluate(inputs),
        CDBSource::IntegerUnit,
        integer
    );
}

void test_preference_changes_only_after_latch() {
    CommonDataBus bus;
    const FUResult integer = make_result(make_tag(9U), 90U);
    const FUResult load = make_result(make_tag(10U), 100U);

    CDBInputs inputs{};
    inputs.integer_result = integer;
    inputs.load_result = load;

    expect_selected(
        bus.evaluate(inputs),
        CDBSource::IntegerUnit,
        integer
    );
    expect_selected(
        bus.evaluate(inputs),
        CDBSource::IntegerUnit,
        integer
    );

    bus.latch();
    expect_selected(bus.evaluate(inputs), CDBSource::LoadUnit, load);
}

void test_single_integer_grant_affects_next_competition() {
    CommonDataBus bus;
    const FUResult integer = make_result(make_tag(11U), 110U);
    const FUResult load = make_result(make_tag(12U), 120U);

    CDBInputs integer_only{};
    integer_only.integer_result = integer;
    expect_selected(
        bus.evaluate(integer_only),
        CDBSource::IntegerUnit,
        integer
    );
    bus.latch();

    CDBInputs competition{};
    competition.integer_result = integer;
    competition.load_result = load;
    expect_selected(
        bus.evaluate(competition),
        CDBSource::LoadUnit,
        load
    );
}

void test_reset_restores_integer_priority() {
    CommonDataBus bus;
    const FUResult integer = make_result(make_tag(13U), 130U);
    const FUResult load = make_result(make_tag(14U), 140U);

    CDBInputs inputs{};
    inputs.integer_result = integer;
    inputs.load_result = load;

    (void)bus.evaluate(inputs);
    bus.latch();
    expect_selected(bus.evaluate(inputs), CDBSource::LoadUnit, load);

    bus.reset();
    expect_selected(
        bus.evaluate(inputs),
        CDBSource::IntegerUnit,
        integer
    );
}

}  // namespace

int main(int argc, char* argv[]) {
    test::TestLog log(CDB_TEST_LOG_PATH);
    if (!log.is_open()) {
        std::cerr << "failed to open CDB test log: "
                  << CDB_TEST_LOG_PATH << '\n';
        return EXIT_FAILURE;
    }
    std::cout << "log: " << CDB_TEST_LOG_PATH << '\n';

    struct TestCase {
        std::string_view name;
        void (*run)();
    };

    const TestCase cases[]{
        {"invalid", test_empty_and_invalid_candidates_are_ignored},
        {"integer", test_integer_candidate_is_broadcast},
        {"load", test_load_candidate_is_broadcast},
        {"competition", test_competition_grants_exactly_one_producer},
        {"round-robin", test_round_robin_alternates_after_latch},
        {"latch", test_preference_changes_only_after_latch},
        {"single-source", test_single_integer_grant_affects_next_competition},
        {"reset", test_reset_restores_integer_priority},
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
        std::cerr << "unknown CDB test case: " << filter << '\n';
        std::cerr << "available cases:";
        for (const TestCase& test_case : cases) {
            std::cerr << ' ' << test_case.name;
        }
        std::cerr << '\n';
        return EXIT_FAILURE;
    }

    if (test::failure_count != 0) {
        std::cerr << test::failure_count << " CDB test(s) failed\n";
        return EXIT_FAILURE;
    }

    std::cout << "all CDB unit tests passed\n";
    return EXIT_SUCCESS;
}
