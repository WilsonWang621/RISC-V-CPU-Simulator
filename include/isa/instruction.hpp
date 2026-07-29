#pragma once

#include "common/types.hpp"

enum class OP { //37 种项目所需指令
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
    LB,
    LH,
    LW,
    LBU,
    LHU,

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

    // S-type
    SB,
    SH,
    SW,

    // J-type
    JAL,

    // 题目约定的停止标记；它不是 RV32I 中独立编码的真实指令。
    HALT,
    INVALID  //ecall/ebreak
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
    //执行运算时，两个输入操作数分别从哪里来
    OperandSource lhs_source = OperandSource::Zero;
    OperandSource rhs_source = OperandSource::Zero;

    bool uses_rs1 = false;
    bool uses_rs2 = false;
    bool writes_rd = false;
    bool is_load = false;    //取
    bool is_store = false;   //存
    bool is_branch = false;  //if判断PC跳转
    bool is_jump = false;    //无条件PC跳转
};
