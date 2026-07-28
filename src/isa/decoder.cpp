#include"isa/decoder.hpp"

namespace{
    constexpr Word kOpcodeMask = 0x7fU;
    constexpr InstructionBits kHaltEncoding = 0x0ff00513U;

    constexpr Word kLoadOpcode = 0x03U;         //I  000 0011
    constexpr Word kOpImmediateOpcode = 0x13U;  //I  001 0011
    constexpr Word kAuipcOpcode = 0x17U;        //U  001 0111
    constexpr Word kStoreOpcode = 0x23U;        //S  010 0011
    constexpr Word kOpOpcode = 0x33U;           //R  011 0011
    constexpr Word kLuiOpcode = 0x37U;          //U  011 0111
    constexpr Word kBranchOpcode = 0x63U;       //B  110 0011
    constexpr Word kJalrOpcode = 0x67U;         //I  110 0111
    constexpr Word kJalOpcode = 0x6fU;          //J  110 1111

    constexpr Word kBaseFunct7 = 0x00U;
    constexpr Word kAlternateFunct7 = 0x20U;

    RegisterIndex decode_register(InstructionBits bits, unsigned high, unsigned low){
        return static_cast<RegisterIndex>(extract_bits(bits, high, low));
    }

    bool write_register(RegisterIndex rd){
        return rd != RegisterIndex{0};
    }
}
DecodedInstruction Decoder::decode(InstructionBits bits){
    if(bits == 0x0ff00513U){
        DecodedInstruction result;
        result.op = OP::HALT;
        return result;
    }

    const Word opcode = extract_bits(bits, 6, 0);

    switch (opcode){
        case kOpOpcode: return decode_R_type(bits);

        case kLoadOpcode:
        case kOpImmediateOpcode:
        case kJalrOpcode:
            return decode_I_type(bits);
        
        case kStoreOpcode:
            return decode_S_type(bits);

        case kBranchOpcode:
            return decode_B_type(bits);

        case kLuiOpcode:
        case kAuipcOpcode:
            return decode_U_type(bits);
  
        case kJalOpcode:
            return decode_J_type(bits);

        default:
            return DecodedInstruction{}; 
    }
}

DecodedInstruction Decoder::decode_R_type(InstructionBits bits){
    const Word funct3 = extract_bits(bits, 14U, 12U);
    const Word funct7 = extract_bits(bits, 31U, 25U);

    OP op = OP::INVALID;
    switch (funct3){
        case 0x0U:
            if (funct7 == kBaseFunct7) {
                op = OP::ADD;
            } else if (funct7 == kAlternateFunct7) {
                op = OP::SUB;
            }
            break;

        case 0x1U:
            if (funct7 == kBaseFunct7) {
                op = OP::SLL;
            }
            break;

        case 0x2U:
            if (funct7 == kBaseFunct7) {
                op = OP::SLT;
            }
            break;

        case 0x3U:
            if (funct7 == kBaseFunct7) {
                op = OP::SLTU;
            }
            break;

        case 0x4U:
            if (funct7 == kBaseFunct7) {
                op = OP::XOR;
            }
            break;

        case 0x5U:
            if (funct7 == kBaseFunct7) {
                op = OP::SRL;
            } else if (funct7 == kAlternateFunct7) {
                op = OP::SRA;
            }
            break;

        case 0x6U:
            if (funct7 == kBaseFunct7) {
                op = OP::OR;
            }
            break;

        case 0x7U:
            if (funct7 == kBaseFunct7) {
                op = OP::AND;
            }
            break;

        default:
            break;
    }      

    if(op == OP::INVALID) return DecodedInstruction{};

    DecodedInstruction result;
    result.op = op;
    result.rd = decode_register(bits, 11U, 7U);
    result.rs1 = decode_register(bits, 19U, 15U);
    result.rs2 = decode_register(bits, 24U, 20U);

    result.lhs_source = OperandSource::Register;
    result.rhs_source = OperandSource::Register;

    result.uses_rs1 = true;
    result.uses_rs2 = true;
    result.writes_rd = write_register(result.rd);

    return result;
}

DecodedInstruction Decoder::decode_I_type(InstructionBits bits){
    const Word opcode = extract_bits(bits, 6U, 0U);
    const Word funct3 = extract_bits(bits, 14U, 12U);
    const Word funct7 = extract_bits(bits, 31U, 25U);

    DecodedInstruction result;

    if (opcode == kOpImmediateOpcode) {
        switch (funct3) {
            case 0x0U:
                result.op = OP::ADDI;
                break;

            case 0x1U:
                if (funct7 != kBaseFunct7) {
                    return DecodedInstruction{};
                }

                result.op = OP::SLLI;
                result.immediate = extract_bits(bits, 24U, 20U);
                break;

            case 0x2U:
                result.op = OP::SLTI;
                break;

            case 0x3U:
                result.op = OP::SLTIU;
                break;

            case 0x4U:
                result.op = OP::XORI;
                break;

            case 0x5U:
                if (funct7 == kBaseFunct7) {
                    result.op = OP::SRLI;
                } else if (funct7 == kAlternateFunct7) {
                    result.op = OP::SRAI;
                } else {
                    return DecodedInstruction{};
                }

                result.immediate = extract_bits(bits, 24U, 20U);
                break;

            case 0x6U:
                result.op = OP::ORI;
                break;

            case 0x7U:
                result.op = OP::ANDI;
                break;

            default:
                return DecodedInstruction{};
        }

        result.rd = decode_register(bits, 11U, 7U);
        result.rs1 = decode_register(bits, 19U, 15U);

        if (result.op != OP::SLLI && result.op != OP::SRLI && result.op != OP::SRAI) {
            result.immediate = decode_I_immediate(bits);
        }

        result.lhs_source = OperandSource::Register;
        result.rhs_source = OperandSource::Immediate;

        result.uses_rs1 = true;
        result.writes_rd = write_register(result.rd);

        return result;
    }

    if(opcode == kLoadOpcode){
        switch (funct3){
            case 0x0U:
                result.op = OP::LB;
                break;

            case 0x1U:
                result.op = OP::LH;
                break;

            case 0x2U:
                result.op = OP::LW;
                break;

            case 0x4U:
                result.op = OP::LBU;
                break;

            case 0x5U:
                result.op = OP::LHU;
                break;

            default:
                return DecodedInstruction{};
        }

        result.immediate = decode_I_immediate(bits);
        result.rd = decode_register(bits, 11U, 7U);
        result.rs1 = decode_register(bits, 19U, 15U);

        result.lhs_source = OperandSource::Register;
        result.rhs_source = OperandSource::Immediate;

        result.is_load = true;
        result.uses_rs1 = true;
        result.writes_rd = write_register(result.rd);

        return result;
    }

    if (opcode == kJalrOpcode) {
        if (funct3 != 0x0U) {
            return DecodedInstruction{};
        }

        result.op = OP::JALR;
        result.rd = decode_register(bits, 11U, 7U);
        result.rs1 = decode_register(bits, 19U, 15U);
        result.immediate = decode_I_immediate(bits);

        result.lhs_source = OperandSource::Register;
        result.rhs_source = OperandSource::Immediate;

        result.uses_rs1 = true;
        result.writes_rd = write_register(result.rd);
        result.is_jump = true;

        return result;
    }

    return DecodedInstruction{};
}

DecodedInstruction Decoder::decode_S_type(InstructionBits bits){
    const Word funct3 = extract_bits(bits, 14U, 12U);

    OP op = OP::INVALID;

    switch (funct3) {
        case 0x0U:
            op = OP::SB;
            break;

        case 0x1U:
            op = OP::SH;
            break;

        case 0x2U:
            op = OP::SW;
            break;

        default:
            return DecodedInstruction{};
    }

    DecodedInstruction result;
    result.op = op;
    result.rs1 = decode_register(bits, 19U, 15U);
    result.rs2 = decode_register(bits, 24U, 20U);
    result.immediate = decode_S_immediate(bits);

    result.lhs_source = OperandSource::Register;
    result.rhs_source = OperandSource::Immediate;

    result.uses_rs1 = true;
    result.uses_rs2 = true;
    result.is_store = true;

    return result;
}

DecodedInstruction Decoder::decode_B_type(InstructionBits bits){
    const Word funct3 = extract_bits(bits, 14U, 12U);

    OP op = OP::INVALID;

    switch (funct3) {
        case 0x0U:
            op = OP::BEQ;
            break;

        case 0x1U:
            op = OP::BNE;
            break;

        case 0x4U:
            op = OP::BLT;
            break;

        case 0x5U:
            op = OP::BGE;
            break;

        case 0x6U:
            op = OP::BLTU;
            break;

        case 0x7U:
            op = OP::BGEU;
            break;

        default:
            return DecodedInstruction{};
    }

    DecodedInstruction result;
    result.op = op;
    result.rs1 = decode_register(bits, 19U, 15U);
    result.rs2 = decode_register(bits, 24U, 20U);
    result.immediate = decode_B_immediate(bits);

    result.lhs_source = OperandSource::Register;
    result.rhs_source = OperandSource::Register;

    result.uses_rs1 = true;
    result.uses_rs2 = true;
    result.is_branch = true;

    return result;
}

DecodedInstruction Decoder::decode_J_type(InstructionBits bits){
    DecodedInstruction result;

    result.op = OP::JAL;
    result.rd = decode_register(bits, 11U, 7U);
    result.immediate = decode_J_immediate(bits);

    result.lhs_source = OperandSource::ProgramCounter;
    result.rhs_source = OperandSource::Immediate;

    result.writes_rd = write_register(result.rd);
    result.is_jump = true;

    return result;
}

Word Decoder::decode_I_immediate(InstructionBits bits){
    const Word immediate = extract_bits(bits, 31U, 20U);
    return sign_extend(immediate, 12);
}

Word Decoder::decode_S_immediate(InstructionBits bits){
    const Word high = extract_bits(bits, 31U, 25U);
    const Word low = extract_bits(bits, 11U, 7U);

    const Word immediate = (high << 5U) | low;
    return sign_extend(immediate, 12U);
}

Word Decoder::decode_B_immediate(InstructionBits bits){
    Word immediate = 0;
    if(get_bit(bits, 31U)){
        immediate |= Word{1} << 12U;
    }

    if(get_bit(bits, 7U)){
        immediate |= Word{1} << 11U;
    }

    immediate |= extract_bits(bits, 30U, 25U) << 5U;
    immediate |= extract_bits(bits, 11U, 8U) << 1U;

    return sign_extend(immediate, 13U);
}

Word Decoder::decode_U_immediate(InstructionBits bits){
    return bits & 0xfffff000U;
}

Word Decoder::decode_J_immediate(InstructionBits bits) {
    Word immediate = 0;

    if (get_bit(bits, 31U)) {
        immediate |= Word{1} << 20U;
    }

    immediate |= extract_bits(bits, 19U, 12U) << 12U;

    if (get_bit(bits, 20U)) {
        immediate |= Word{1} << 11U;
    }

    immediate |= extract_bits(bits, 30U, 21U) << 1U;

    return sign_extend(immediate, 21U);
}