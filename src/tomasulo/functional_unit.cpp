#include"functional_unit.hpp"

#include <cassert>

#include "common/config.hpp"

namespace {

constexpr Word kShiftMask = 0x1fU;       // 0001 1111
constexpr Word kClearLeastSignificantBit = ~Word{1};  //全1

Address sequential_pc(Address pc) noexcept {
    return pc + static_cast<Address>(kInstructionWidth);
}

} 

FunctionalUnit::FunctionalUnit(){
    reset();
}

void FunctionalUnit::reset(){
    current_result_ = FUResult{};
    next_result_ = FUResult{};
}

void FunctionalUnit::latch(){
    current_result_ = next_result_;
}

bool FunctionalUnit::has_result() const{
    return current_result_.valid;
}

FUResult FunctionalUnit::result() const{
    return current_result_;
}

bool FunctionalUnit::can_accept(bool result_accepted) const{
    // 当前没有结果，或者当前结果本周期会被 CDB 取走，
    // 都意味着周期末有空间保存新结果。
    return result_accepted || !current_result_.valid;
}

bool FunctionalUnit::signed_greater_equal(Word lhs, Word rhs){
    return static_cast<SignedWord>(lhs) >= static_cast<SignedWord>(rhs);
}

bool FunctionalUnit::signed_less_than(Word lhs, Word rhs){
    return static_cast<SignedWord>(lhs) < static_cast<SignedWord>(rhs);
}

Word FunctionalUnit::arithmetic_shift_right( Word value, unsigned amount) {
    amount &= 31U;

    if (amount == 0U) {
        return value;
    }

    Word result = value >> amount;

    if ((value & 0x80000000U) != 0U) {
        result |= ~Word{0} << (32U - amount);
    }

    return result;
}

FUResult FunctionalUnit::execute_instruction(const Execute& execute){
    FUResult output{};

    output.tag = execute.tag;
    output.actual_next_pc = sequential_pc(execute.pc);
    
    const Word lhs = execute.operand1;
    const Word rhs = execute.operand2;

    switch (execute.op){
        case OP::ADD:
        case OP::ADDI:
            output.result = lhs + rhs;   //为什么没有immediate?
            break;
        case OP::AND:
        case OP::ANDI:
            output.result = lhs & rhs;
            break;
        case OP::AUIPC: //?
            output.result = lhs + rhs;
            break;
        case OP::BEQ:
            output.branch_taken = lhs == rhs;
            break;
        case OP::BGE:
            output.branch_taken = signed_greater_equal(lhs, rhs);
            output.has_control_result = true;
            break;
        case OP::BGEU:
            output.branch_taken = lhs >= rhs;
            output.has_control_result = true;
            break;
        case OP::BLT:
            output.branch_taken = signed_less_than(lhs, rhs);
            output.has_control_result = true;
            break;
        case OP::BLTU:
            output.branch_taken = lhs < rhs;
            output.has_control_result = true;
            break;
        case OP::BNE:
            output.branch_taken = lhs != rhs;
            output.has_control_result = true;
            break;
        case OP::JAL:
            output.result = sequential_pc(execute.pc);
            output.actual_next_pc = execute.pc + static_cast<Address>(execute.immediate);
            output.branch_taken = true;
            output.has_control_result = true;
            output.valid = true;
            return output;
        case OP::JALR:
            output.result = sequential_pc(execute.pc);
            output.actual_next_pc = static_cast<Address>((lhs + execute.immediate) & kClearLeastSignificantBit);
            output.branch_taken = true;
            output.has_control_result = true;
            output.valid = true;
            return output;
        case OP::OR:
        case OP::ORI:
            output.result = lhs | rhs;
            break;
        case OP::SLL:
        case OP::SLLI:
            const unsigned amount = static_cast<unsigned>(rhs & kShiftMask);
            output.result = lhs << amount;
            break;
        case OP::SLTU:
        case OP::SLTIU:
            output.result = lhs < rhs ? Word{1} : Word{0};
            break;
        case OP::SLT:
        case OP::SLTI:
            output.result = signed_less_than(lhs, rhs) ? Word{1} : Word{0};
            break;
        case OP::SRA:
        case OP::SRAI:
            const unsigned amount = static_cast<unsigned>(rhs & kShiftMask);
            output.result = arithmetic_shift_right(lhs, amount);
            break;
        case OP::SRL:
        case OP::SRLI:
            const unsigned amount = static_cast<unsigned>(rhs & kShiftMask);
            output.result = lhs >> amount;
            break;
        case OP::SUB:
            output.result = lhs - rhs;
            break;
        case OP::XOR:
        case OP::XORI:
            output.result = lhs ^ rhs;
            break;
        case OP::LB:
        case OP::LH:
        case OP::LW:
        case OP::LBU:
        case OP::LHU:
        case OP::SB:
        case OP::SH:
        case OP::SW:
        case OP::HALT:
        case OP::INVALID:
            return FUResult{};
    }
    if(output.has_control_result){
        output.actual_next_pc = output.branch_taken ? execute.pc + static_cast<Address>(execute.immediate) : sequential_pc(execute.pc);
    }

    output.valid = true;
    return output;
}

bool FunctionalUnit::evaluate(Execute& execute, bool flush, bool result_accepted){
    next_result_ = current_result_;
    // Flush 优先级最高：当前结果和本周期新输入都作废
    if(flush){
        next_result_ = FUResult{};
        return false;
    }

    // 当前结果获得 CDB 仲裁，可以从 FU 中删除
    if(result_accepted && current_result_.valid){
        next_result_ = FUResult{};
    }

    if(!execute.valid){
        return false;
    }

    if (!can_accept(result_accepted)) {
        // 输出寄存器仍被旧结果占用，不能覆盖。
        return false;
    }

    FUResult computed = execute_instruction(execute);

    // RS 不应该把 Load、Store、HALT 或 INVALID 发给整数 FU。
    assert(computed.valid);

    if(!computed.valid) return false;

    next_result_ = computed;
    return true;
}