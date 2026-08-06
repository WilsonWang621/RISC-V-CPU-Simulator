#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string_view>

#include "test_log.hpp"
#include "test_support.hpp"
#include "tomasulo/branch_predictor.hpp"

#ifndef BRANCH_PREDICTOR_TEST_LOG_PATH
#define BRANCH_PREDICTOR_TEST_LOG_PATH "riscv_branch_predictor_unit_tests.log"
#endif

namespace {

BranchPredictorUpdate make_update(
    Address pc,
    bool actual_taken,
    Address predicted_next_pc,
    Address actual_next_pc,
    bool valid = true
) {
    BranchPredictorUpdate update{};
    update.pc = pc;
    update.actual_taken = actual_taken;
    update.predicted_next_pc = predicted_next_pc;
    update.actual_next_pc = actual_next_pc;
    update.valid = valid;
    return update;
}

void apply_update(
    BranchPredictor& predictor,
    const BranchPredictorUpdate& update
) {
    predictor.apply(update);
    predictor.latch();
}

void test_constructor_initializes_weak_not_taken() {
    BranchPredictor predictor;

    for (
        std::size_t index = 0U;
        index < BranchPredictor::kTableSize;
        ++index
    ) {
        const Address pc = static_cast<Address>(index * 4U);
        EXPECT_EQ(predictor.counter(pc), std::uint8_t{1U});
        EXPECT_FALSE(predictor.predict_taken(pc));
    }

    EXPECT_EQ(predictor.prediction_count(), CycleCount{0});
    EXPECT_EQ(predictor.correct_count(), CycleCount{0});
    EXPECT_EQ(predictor.accuracy(), 0.0);
}

void test_update_is_visible_only_after_latch() {
    BranchPredictor predictor;
    constexpr Address pc = 0x100U;

    predictor.apply(
        make_update(pc, true, pc + 4U, pc + 8U)
    );

    EXPECT_EQ(predictor.counter(pc), std::uint8_t{1U});
    EXPECT_FALSE(predictor.predict_taken(pc));
    EXPECT_EQ(predictor.prediction_count(), CycleCount{0});

    predictor.latch();

    EXPECT_EQ(predictor.counter(pc), std::uint8_t{2U});
    EXPECT_TRUE(predictor.predict_taken(pc));
    EXPECT_EQ(predictor.prediction_count(), CycleCount{1});
}

void test_invalid_update_preserves_all_state() {
    BranchPredictor predictor;
    constexpr Address pc = 0x180U;

    apply_update(
        predictor,
        make_update(pc, true, 0x184U, 0x184U)
    );

    const std::uint8_t counter_before = predictor.counter(pc);
    const CycleCount predictions_before = predictor.prediction_count();
    const CycleCount correct_before = predictor.correct_count();

    predictor.apply(
        make_update(pc, false, 0x184U, 0x188U, false)
    );
    predictor.latch();

    EXPECT_EQ(predictor.counter(pc), counter_before);
    EXPECT_EQ(predictor.prediction_count(), predictions_before);
    EXPECT_EQ(predictor.correct_count(), correct_before);
}

void test_counter_saturates_in_both_directions() {
    BranchPredictor predictor;
    constexpr Address pc = 0x200U;

    apply_update(predictor, make_update(pc, false, 0U, 4U));
    EXPECT_EQ(predictor.counter(pc), std::uint8_t{0U});
    EXPECT_FALSE(predictor.predict_taken(pc));

    apply_update(predictor, make_update(pc, false, 0U, 4U));
    EXPECT_EQ(predictor.counter(pc), std::uint8_t{0U});

    apply_update(predictor, make_update(pc, true, 0U, 4U));
    EXPECT_EQ(predictor.counter(pc), std::uint8_t{1U});
    EXPECT_FALSE(predictor.predict_taken(pc));

    apply_update(predictor, make_update(pc, true, 0U, 4U));
    EXPECT_EQ(predictor.counter(pc), std::uint8_t{2U});
    EXPECT_TRUE(predictor.predict_taken(pc));

    apply_update(predictor, make_update(pc, true, 0U, 4U));
    EXPECT_EQ(predictor.counter(pc), std::uint8_t{3U});
    EXPECT_TRUE(predictor.predict_taken(pc));

    apply_update(predictor, make_update(pc, true, 0U, 4U));
    EXPECT_EQ(predictor.counter(pc), std::uint8_t{3U});
}

void test_only_indexed_entry_changes() {
    BranchPredictor predictor;
    constexpr Address updated_pc = 0x300U;
    constexpr Address neighbor_pc = updated_pc + 4U;

    apply_update(
        predictor,
        make_update(updated_pc, true, updated_pc + 4U, updated_pc + 8U)
    );

    EXPECT_EQ(predictor.counter(updated_pc), std::uint8_t{2U});
    EXPECT_EQ(predictor.counter(neighbor_pc), std::uint8_t{1U});
}

void test_pc_indexing_ignores_low_bits_and_aliases_by_table_size() {
    BranchPredictor predictor;
    constexpr Address pc = 0x104U;
    constexpr Address same_word = pc + 3U;
    constexpr Address alias = pc
        + static_cast<Address>(BranchPredictor::kTableSize * 4U);
    constexpr Address neighbor = pc + 4U;

    apply_update(
        predictor,
        make_update(pc, true, pc + 4U, pc + 8U)
    );

    EXPECT_EQ(predictor.counter(pc), std::uint8_t{2U});
    EXPECT_EQ(predictor.counter(same_word), std::uint8_t{2U});
    EXPECT_EQ(predictor.counter(alias), std::uint8_t{2U});
    EXPECT_EQ(predictor.counter(neighbor), std::uint8_t{1U});
}

void test_statistics_are_updated_after_latch() {
    BranchPredictor predictor;

    predictor.apply(
        make_update(0x400U, true, 0x480U, 0x480U)
    );
    EXPECT_EQ(predictor.prediction_count(), CycleCount{0});
    EXPECT_EQ(predictor.correct_count(), CycleCount{0});
    predictor.latch();

    EXPECT_EQ(predictor.prediction_count(), CycleCount{1});
    EXPECT_EQ(predictor.correct_count(), CycleCount{1});
    EXPECT_EQ(predictor.accuracy(), 1.0);

    predictor.apply(
        make_update(0x404U, false, 0x408U, 0x440U)
    );
    EXPECT_EQ(predictor.prediction_count(), CycleCount{1});
    EXPECT_EQ(predictor.correct_count(), CycleCount{1});
    predictor.latch();

    EXPECT_EQ(predictor.prediction_count(), CycleCount{2});
    EXPECT_EQ(predictor.correct_count(), CycleCount{1});
    EXPECT_EQ(predictor.accuracy(), 0.5);
}

void test_reset_restores_counters_and_statistics() {
    BranchPredictor predictor;
    constexpr Address pc = 0x500U;

    apply_update(
        predictor,
        make_update(pc, true, pc + 8U, pc + 8U)
    );
    apply_update(
        predictor,
        make_update(pc, true, pc + 8U, pc + 8U)
    );

    EXPECT_EQ(predictor.counter(pc), std::uint8_t{3U});
    EXPECT_EQ(predictor.prediction_count(), CycleCount{2});
    EXPECT_EQ(predictor.correct_count(), CycleCount{2});

    predictor.reset();

    EXPECT_EQ(predictor.counter(pc), std::uint8_t{1U});
    EXPECT_FALSE(predictor.predict_taken(pc));
    EXPECT_EQ(predictor.prediction_count(), CycleCount{0});
    EXPECT_EQ(predictor.correct_count(), CycleCount{0});
    EXPECT_EQ(predictor.accuracy(), 0.0);
}

}  // namespace

int main(int argc, char* argv[]) {
    test::TestLog log(BRANCH_PREDICTOR_TEST_LOG_PATH);
    if (!log.is_open()) {
        std::cerr << "failed to open branch-predictor test log: "
                  << BRANCH_PREDICTOR_TEST_LOG_PATH << '\n';
        return EXIT_FAILURE;
    }
    std::cout << "log: " << BRANCH_PREDICTOR_TEST_LOG_PATH << '\n';

    struct TestCase {
        std::string_view name;
        void (*run)();
    };

    const TestCase cases[]{
        {"initialization", test_constructor_initializes_weak_not_taken},
        {"latch", test_update_is_visible_only_after_latch},
        {"invalid", test_invalid_update_preserves_all_state},
        {"saturation", test_counter_saturates_in_both_directions},
        {"independence", test_only_indexed_entry_changes},
        {"indexing", test_pc_indexing_ignores_low_bits_and_aliases_by_table_size},
        {"statistics", test_statistics_are_updated_after_latch},
        {"reset", test_reset_restores_counters_and_statistics},
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
        std::cerr << "unknown branch-predictor test case: " << filter << '\n';
        std::cerr << "available cases:";
        for (const TestCase& test_case : cases) {
            std::cerr << ' ' << test_case.name;
        }
        std::cerr << '\n';
        return EXIT_FAILURE;
    }

    if (test::failure_count != 0) {
        std::cerr << test::failure_count
                  << " branch-predictor test(s) failed\n";
        return EXIT_FAILURE;
    }

    std::cout << "all branch-predictor unit tests passed\n";
    return EXIT_SUCCESS;
}
