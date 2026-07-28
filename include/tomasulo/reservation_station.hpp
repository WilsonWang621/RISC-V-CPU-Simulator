#pragma once

#include "common/types.hpp"
#include "isa/instruction.hpp"
#include "tomasulo/signals.hpp"

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
