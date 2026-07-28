#pragma once

#include "common/types.hpp"
#include "isa/instruction.hpp"

// ROB 只保存执行完成、顺序提交和分支恢复所需的信息。
// RS/LSQ 中的操作数等待状态不应复制到这里。
struct ROBEntry {
    RobTag tag{};
    OP op = OP::INVALID;
    Address pc = 0;

    RegisterIndex rd = 0;
    Word value = 0;
    bool writes_rd = false;
    bool ready = false;

    Address predicted_next_pc = 0;
    Address actual_next_pc = 0;
    bool is_branch = false;
    bool is_store = false;

    bool valid = false;
};
