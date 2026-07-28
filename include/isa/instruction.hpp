#pragma once

#include "common/types.hpp"

enum class OP {
    // U-type
    LUI,
    AUIPC,

    // I-type
    ADDI,
    SLTI,
    SLTIU,
    XORI,
    ORI,
    ANDI,
    SLLI,
    SRLI,
    SRAI,
    JALR,

    // R-type
    ADD,
    SUB,
    AND,
    XOR,
    OR,
    SLL,
    SLT,
    SLTU,
    SRL,
    SRA,

    // B-type
    BEQ,
    BNE,
    BLT,
    BGE,
    BLTU,
    BGEU,

    // Load
    LB,
    LH,
    LW,
    LBU,
    LHU,

    // S-type
    SB,
    SH,
    SW,

    // J-type
    JAL,

    // 题目约定的停止标记；它不是 RV32I 中独立编码的真实指令。
    HALT,
    INVALID
};

enum class OperandSource {
    Immediate,
    Register,
    ProgramCounter,
    Zero
};

// 这里只保存译码语义；PC 和分支预测属于动态指令实例，放在消息中。
struct DecodedInstruction {
    OP op = OP::INVALID;

    RegisterIndex rd = 0;
    RegisterIndex rs1 = 0;
    RegisterIndex rs2 = 0;
    Word immediate = 0;

    OperandSource lhs_source = OperandSource::Zero;
    OperandSource rhs_source = OperandSource::Zero;

    bool uses_rs1 = false;
    bool uses_rs2 = false;
    bool writes_rd = false;
    bool is_load = false;
    bool is_store = false;
    bool is_branch = false;
    bool is_jump = false;
};
