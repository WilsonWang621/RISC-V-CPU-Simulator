#include"interpreter.hpp"
#include"isa/decoder.hpp"

namespace{
    constexpr Word kShiftMask = 0x1fU;  // 0001 1111
    constexpr Word kLeastSignificantBitMask = ~Word{1}; //清理最低位

    Word arithmetic_shift_right(Word value, unsigned amount) noexcept {
        amount &= 31U; // 1111

        if (amount == 0U) {
            return value;
        }

        Word result = value >> amount;

        if ((value & 0x80000000U) != 0U) {
            result |= ~Word{0} << (32U - amount);
        }

        return result;
    }

    bool signed_less_than(Word lhs, Word rhs) noexcept {
        return static_cast<SignedWord>(lhs) < static_cast<SignedWord>(rhs);
    }

    bool signed_greater_equal(Word lhs, Word rhs) noexcept {
        return static_cast<SignedWord>(lhs) >= static_cast<SignedWord>(rhs);
    }
}
ImageLoadResult Interpreter::load(std::istream &input){
    register_.fill(Word{0});
    pc_ = 0;
    instruction_count_ = 0;
    exit_code_ = 0;
    status_ = Status::kNotLoaded;

    ImageLoadResult result = memory_.load(input);
    
    if(result.ok()){
        status_ = Status::kReady;
    }else{
        status_ = Status::kLoadError;
    }

    return result;
}

Interpreter::StepResult Interpreter::step(){
    if(status_ != Status::kReady || status_ != Status::kRunning){
        return current_result();
    }

    const Address cur_pc = pc_;
    const auto fetched = memory_.read_word(cur_pc); 
    if(!fetched.has_value()){
        status_ = Status::kFetchOutOfBounds;
        return current_result();
    }
    //std::optional 重载了 operator*。它的行为是直接返回其内部存储的对象的引用
    const DecodedInstruction instruction = Decoder::decode(*fetched); //Word&
    
    if(instruction.op == OP::HALT){
        status_ = Status::kHalted;
        exit_code_ = register_[10] & 0xffU;
        return current_result();
    }
    if (instruction.op == OP::INVALID) {
        status_ = Status::kInvalidInstruction;
        return current_result();
    }

    const Word rs1_value = register_[instruction.rs1];
    const Word rs2_value = register_[instruction.rs2];

    Address  next_pc = cur_pc + static_cast<Address>(kInstructionWidth);
    Word writeback_value = 0;
    bool has_writeback = false;

    switch(instruction.op){
        case OP::LUI:
            writeback_value = instruction.immediate;
            has_writeback = true;
            break;

        case OP::AUIPC:
            writeback_value = cur_pc + instruction.immediate;
            has_writeback = true;
            break;

        case OP::ADDI:
            writeback_value = rs1_value + instruction.immediate;
            has_writeback = true;
            break;

        case OP::SLT: //要先转化成有符号数
            writeback_value = signed_less_than(rs1_value, instruction.immediate) ? Word{1} : Word{0};
            has_writeback = true;
            break;

        case OP::SLTI:
            writeback_value = signed_less_than(
                rs1_value,
                instruction.immediate
            ) ? Word{1} : Word{0};
            has_writeback = true;
            break;

        case OP::SLTU:
            writeback_value = rs1_value < instruction.immediate ? Word{1} : Word{0};
            has_writeback = true;
            break;

        case OP::SLTIU:
            writeback_value = rs1_value < instruction.immediate ? Word{1} : Word{0};
            has_writeback = true;
            break;
            

        case OP::XORI:
            writeback_value = rs1_value ^ instruction.immediate;
            has_writeback = true;
            break;

        case OP::ORI:
            writeback_value = rs1_value | instruction.immediate;
            has_writeback = true;
            break;

        case OP::ANDI:
            writeback_value = rs1_value & instruction.immediate;
            has_writeback = true;
            break;

        case OP::SLLI: {
            const unsigned shift = static_cast<unsigned>(
                instruction.immediate & kShiftMask
            );
            writeback_value = rs1_value << shift;
            has_writeback = true;
            break;
        }

        case OP::SRLI: {
            const unsigned shift = static_cast<unsigned>(
                instruction.immediate & kShiftMask
            );
            writeback_value = rs1_value >> shift;
            has_writeback = true;
            break;
        }

        case OP::SRAI: {
            const unsigned shift = static_cast<unsigned>(
                instruction.immediate & kShiftMask
            );
            writeback_value = arithmetic_shift_right(rs1_value, shift);
            has_writeback = true;
            break;
        }

        case OP::ADD:
            writeback_value = rs1_value + rs2_value;
            has_writeback = true;
            break;

        case OP::SUB:
            writeback_value = rs1_value - rs2_value;
            has_writeback = true;
            break;

        case OP::AND:
            writeback_value = rs1_value & rs2_value;
            has_writeback = true;
            break;

        case OP::XOR:
            writeback_value = rs1_value ^ rs2_value;
            has_writeback = true;
            break;

        case OP::OR:
            writeback_value = rs1_value | rs2_value;
            has_writeback = true;
            break;

        case OP::SLL: {
            const unsigned shift = static_cast<unsigned>(
                rs2_value & kShiftMask
            );
            writeback_value = rs1_value << shift;
            has_writeback = true;
            break;
        }

        case OP::SRL: {
            const unsigned shift = static_cast<unsigned>(
                rs2_value & kShiftMask
            );
            writeback_value = rs1_value >> shift;
            has_writeback = true;
            break;
        }

        case OP::SRA: {
            const unsigned shift = static_cast<unsigned>(
                rs2_value & kShiftMask
            );
            writeback_value = arithmetic_shift_right(rs1_value, shift);
            has_writeback = true;
            break;
        }

        case OP::JAL:
            writeback_value = cur_pc + static_cast<Address>(kInstructionWidth);
            has_writeback = true;
            next_pc = cur_pc + instruction.immediate;
            break;

        case OP::JALR:
            // 目标必须使用写回前保存的 rs1_value 计算。
            writeback_value =
                cur_pc + static_cast<Address>(kInstructionWidth);
            has_writeback = true;
            next_pc =
                (rs1_value + instruction.immediate)
                & kLeastSignificantBitMask;
            break;

        case OP::BEQ:
            if (rs1_value == rs2_value) {
                next_pc = cur_pc + instruction.immediate;
            }
            break;

        case OP::BNE:
            if (rs1_value != rs2_value) {
                next_pc = cur_pc + instruction.immediate;
            }
            break;

        case OP::BLT:
            if (signed_less_than(rs1_value, rs2_value)) {
                next_pc = cur_pc + instruction.immediate;
            }
            break;

        case OP::BGE:
            if (signed_greater_equal(rs1_value, rs2_value)) {
                next_pc = cur_pc + instruction.immediate;
            }
            break;

        case OP::BLTU:
            if (rs1_value < rs2_value) {
                next_pc = cur_pc + instruction.immediate;
            }
            break;

        case OP::BGEU:
            if (rs1_value >= rs2_value) {
                next_pc = cur_pc + instruction.immediate;
            }
            break;

        case OP::LB: {
            const Address address = rs1_value + instruction.immediate;
            const auto value = memory_.read_byte(address);

            if (!value.has_value()) {
                status_ = Status::kMemoryOutOfBounds;
                return current_result();
            }

            writeback_value = sign_extend(
                static_cast<Word>(*value),
                8U
            );
            has_writeback = true;
            break;
        }

        case OP::LH: {
            const Address address = rs1_value + instruction.immediate;
            const auto value = memory_.read_half(address);

            if (!value.has_value()) {
                status_ = Status::kMemoryOutOfBounds;
                return current_result();
            }

            writeback_value = sign_extend(
                static_cast<Word>(*value),
                16U
            );
            has_writeback = true;
            break;
        }

        case OP::LW: {
            const Address address = rs1_value + instruction.immediate;
            const auto value = memory_.read_word(address);

            if (!value.has_value()) {
                status_ = Status::kMemoryOutOfBounds;
                return current_result();
            }

            writeback_value = *value;
            has_writeback = true;
            break;
        }

        case OP::LBU: {
            const Address address = rs1_value + instruction.immediate;
            const auto value = memory_.read_byte(address);

            if (!value.has_value()) {
                status_ = Status::kMemoryOutOfBounds;
                return current_result();
            }

            writeback_value = static_cast<Word>(*value);
            has_writeback = true;
            break;
        }

        case OP::LHU: {
            const Address address = rs1_value + instruction.immediate;
            const auto value = memory_.read_half(address);

            if (!value.has_value()) {
                status_ = Status::kMemoryOutOfBounds;
                return current_result();
            }

            writeback_value = static_cast<Word>(*value);
            has_writeback = true;
            break;
        }

        case OP::SB: {
            const Address address = rs1_value + instruction.immediate;

            if (!memory_.write_byte(
                    address,
                    static_cast<Byte>(rs2_value & 0xffU))) {
                status_ = Status::kMemoryOutOfBounds;
                return current_result();
            }
            break;
        }

        case OP::SH: {
            const Address address = rs1_value + instruction.immediate;

            if (!memory_.write_half(
                    address,
                    static_cast<HalfWord>(rs2_value & 0xffffU))) {
                status_ = Status::kMemoryOutOfBounds;
                return current_result();
            }
            break;
        }

        case OP::SW: {
            const Address address = rs1_value + instruction.immediate;

            if (!memory_.write_word(address, rs2_value)) {
                status_ = Status::kMemoryOutOfBounds;
                return current_result();
            }
            break;
        }

        case OP::INVALID:
        case OP::HALT:
            status_ = Status::kInvalidInstruction;
            return current_result();
    }

    if (has_writeback && instruction.rd != RegisterIndex{0}) {
        register_[instruction.rd] = writeback_value;
    }

    pc_ = next_pc;
    register_[0] = Word{0};
    ++instruction_count_;
    status_ = Status::kRunning;

    return current_result();
}

Interpreter::StepResult Interpreter::run(){
    if (status_ != Status::kReady && status_ != Status::kRunning) {
        return current_result();
    }

    StepResult result = current_result();

    do {
        result = step();
    } while (result.status == Status::kRunning);

    return result;
}
