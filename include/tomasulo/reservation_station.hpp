#pragma once

#include<array>
#include<cstddef>

#include "common/types.hpp"
#include "isa/instruction.hpp"
#include "tomasulo/signals.hpp"

static constexpr std::size_t kCapacity = 16U;

// RS 保存值或依赖 tag。busy 表示该固定槽位是否被一条指令占用。
struct RSEntry {
    OP op = OP::INVALID;
    RobTag destination{};

    Operand lhs{};
    Operand rhs{};

    Word immediate = 0;
    Address pc = 0;
    Address predicted_next_pc = 0;

    bool busy = false;
};

struct RSInputs{
    // Issue Unit 产生的新条目
    RSEntry issue_entry{};
    bool issue_valid = false;

    // 本周期 CDB 广播。
    CDBMsg cdb{};

    // 对应功能单元本周期是否可以接收新指令。
    bool fu_available = false;

    // 分支预测错误时清空 RS。
    bool flush = false;

};

struct RSOutputs{
    //RS->ALU
    Execute dispatch{};
    bool dispatch_valid = false;

    // RS 是否成功接收 Issue Unit 的新条目。
    bool issue_accepted = false;
};

class ReservationStation{
public:
    ReservationStation();

    void reset();

    void latch();

    bool empty() const;
    bool full() const;
    std::size_t size() const;

    // Issue Unit 在发射前用它检查资源。
    bool available() const;

    RSOutputs evaluate(const RSInputs& inputs);


private:
    static constexpr std::size_t kInvalidIndex = kCapacity;

    std::array<RSEntry, kCapacity> cur_rs_{};
    std::array<RSEntry, kCapacity> next_rs_{};

    // 使用轮询仲裁，避免固定低下标槽位长期占据派发优先级。
    std::size_t dispatch_cursor_ = 0U;
    std::size_t next_dispatch_cursor_ = 0U;

    static void wake_operand(Operand& operand, const CDBMsg& cdb); // make operand ready
    static void wake_entry(RSEntry& entry, const CDBMsg& cdb); //use wake_operand to make entry ready

    std::size_t select_ready_index();

    std::size_t find_free_index(const std::array<RSEntry, kCapacity>& entries);

    Execute make_execute(const RSEntry &entry);
};