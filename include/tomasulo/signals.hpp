#pragma once

#include "common/types.hpp"
#include "isa/instruction.hpp"

// 一个源操作数要么已经有值，要么等待某个 ROB tag 的广播。
struct Operand {
    Word value = 0;
    RobTag tag{};
    bool ready = false;
};

// Memory -> Fetch。预测信息必须跟随这一次动态取指继续向后传递。
struct FetchPacket {
    Address pc = 0;
    InstructionBits instruction = 0;
    Address predicted_pc = 0;
    bool predicted_taken = false;
    bool valid = false;
};

// Decoder -> Issue。保留原始指令便于调试，decoded 保存统一译码结果。
struct DecodePacket {
    Address pc = 0;
    InstructionBits instruction = 0;
    DecodedInstruction decoded{};
    Address predicted_pc = 0;
    bool predicted_taken = false;
    bool valid = false;
};

// CDB -> ROB / RS / LSQ。CDB 不需要知道结果来自哪一种指令。
struct CDBMsg {
    RobTag tag{};
    Word value = 0;
    bool valid = false;
};

// Reservation station -> functional unit。进入 FU 前两个操作数都应当 ready。
struct Execute {
    OP op = OP::INVALID;
    Word operand1 = 0;
    Word operand2 = 0;
    Word immediate = 0;
    Address pc = 0;
    RobTag tag{};
    bool valid = false;
};

// Functional unit -> CDB / branch handling。
// 普通 ALU 只使用 result；分支和跳转还要填写实际下一 PC。
struct FUResult {
    RobTag tag{};
    Word result = 0;
    Address actual_next_pc = 0;
    bool branch_taken = false;
    bool has_control_result = false;
    bool valid = false;
};
