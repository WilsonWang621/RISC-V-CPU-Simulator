#pragma once
#include "isa/instruction.hpp"

class Decoder {
public:
    static DecodedInstruction decode(InstructionBits bits);

private:
    static DecodedInstruction decode_R_type(InstructionBits bits);

    static DecodedInstruction decode_U_type(InstructionBits bits);

    static DecodedInstruction decode_I_type(InstructionBits bits);

    static DecodedInstruction decode_B_type(InstructionBits bits);

    static DecodedInstruction decode_S_type(InstructionBits bits);

    static DecodedInstruction decode_J_type(InstructionBits bits);

    static Word decode_I_immediate(InstructionBits bits);

    static Word decode_U_immediate(InstructionBits bits);

    static Word decode_B_immediate(InstructionBits bits);

    static Word decode_S_immediate(InstructionBits bits);

    static Word decode_J_immediate(InstructionBits bits);
};
