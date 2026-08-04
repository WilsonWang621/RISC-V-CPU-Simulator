#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "common/types.hpp"

struct BranchPredictorUpdate {
    Address pc = 0;

    bool actual_taken = false;

    Address predicted_next_pc = 0;
    Address actual_next_pc = 0;

    // 只有已提交的条件分支才能置 true。
    bool valid = false;
};

class BranchPredictor{
public:
    static constexpr std::size_t kTableSize = 256U;

    BranchPredictor();

    // 使用当前周期旧状态进行预测。
    bool predict_taken(Address pc) const;

    // 每周期调用一次；没有分支提交时传 valid=false。
    void evaluate_update(const BranchPredictorUpdate& update);

    void latch();
    void reset();

    CycleCount prediction_count() const;
    CycleCount correct_count() const;
    double accuracy() const;

    // 便于unit test & debug
    std::uint8_t counter(Address pc) const;
private:
    static constexpr std::uint8_t kStrongNotTaken = 0U;
    static constexpr std::uint8_t kWeakNotTaken = 1U;
    static constexpr std::uint8_t kWeakTaken = 2U;
    static constexpr std::uint8_t kStrongTaken = 3U;

    std::array<std::uint8_t, kTableSize> current_{};
    std::array<std::uint8_t, kTableSize> next_{};

    CycleCount prediction_count_ = 0;
    CycleCount next_prediction_count_ = 0;

    CycleCount correct_count_ = 0;
    CycleCount next_correct_count_ = 0;

    static size_t index_of(Address pc);
};