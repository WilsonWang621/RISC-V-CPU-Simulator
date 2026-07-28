#include <array>
#include <cstdint>
#include <iostream>

#include "isa/decoder.hpp"
#include "test_support.hpp"

namespace {

constexpr Word kLoadOpcode = 0x03U;
constexpr Word kOpImmediateOpcode = 0x13U;
constexpr Word kAuipcOpcode = 0x17U;
constexpr Word kStoreOpcode = 0x23U;
constexpr Word kOpOpcode = 0x33U;
constexpr Word kLuiOpcode = 0x37U;
constexpr Word kBranchOpcode = 0x63U;
constexpr Word kJalrOpcode = 0x67U;
constexpr Word kJalOpcode = 0x6fU;

InstructionBits encode_r(
    Word funct7,
    RegisterIndex rs2,
    RegisterIndex rs1,
    Word funct3,
    RegisterIndex rd
) {
    return (funct7 << 25U)
        | (static_cast<Word>(rs2) << 20U)
        | (static_cast<Word>(rs1) << 15U)
        | (funct3 << 12U)
        | (static_cast<Word>(rd) << 7U)
        | kOpOpcode;
}

InstructionBits encode_i(
    Word immediate,
    RegisterIndex rs1,
    Word funct3,
    RegisterIndex rd,
    Word opcode
) {
    return ((immediate & 0xfffU) << 20U)
        | (static_cast<Word>(rs1) << 15U)
        | (funct3 << 12U)
        | (static_cast<Word>(rd) << 7U)
        | opcode;
}

InstructionBits encode_s(
    Word immediate,
    RegisterIndex rs2,
    RegisterIndex rs1,
    Word funct3
) {
    const Word encoded = immediate & 0xfffU;
    return ((encoded >> 5U) << 25U)
        | (static_cast<Word>(rs2) << 20U)
        | (static_cast<Word>(rs1) << 15U)
        | (funct3 << 12U)
        | ((encoded & 0x1fU) << 7U)
        | kStoreOpcode;
}

InstructionBits encode_b(
    Word immediate,
    RegisterIndex rs2,
    RegisterIndex rs1,
    Word funct3
) {
    const Word encoded = immediate & 0x1fffU;
    return (((encoded >> 12U) & 0x1U) << 31U)
        | (((encoded >> 5U) & 0x3fU) << 25U)
        | (static_cast<Word>(rs2) << 20U)
        | (static_cast<Word>(rs1) << 15U)
        | (funct3 << 12U)
        | (((encoded >> 1U) & 0xfU) << 8U)
        | (((encoded >> 11U) & 0x1U) << 7U)
        | kBranchOpcode;
}

InstructionBits encode_u(Word immediate, RegisterIndex rd, Word opcode) {
    return (immediate & 0xfffff000U)
        | (static_cast<Word>(rd) << 7U)
        | opcode;
}

InstructionBits encode_j(Word immediate, RegisterIndex rd) {
    const Word encoded = immediate & 0x1fffffU;
    return (((encoded >> 20U) & 0x1U) << 31U)
        | (((encoded >> 1U) & 0x3ffU) << 21U)
        | (((encoded >> 11U) & 0x1U) << 20U)
        | (((encoded >> 12U) & 0xffU) << 12U)
        | (static_cast<Word>(rd) << 7U)
        | kJalOpcode;
}

void expect_op(OP actual, OP expected, int line) {
    if (actual == expected) {
        return;
    }

    std::cerr << __FILE__ << ':' << line
              << ": unexpected OP value; actual="
              << static_cast<int>(actual)
              << ", expected=" << static_cast<int>(expected) << '\n';
    ++test::failure_count;
}

#define EXPECT_OP(actual, expected) expect_op((actual), (expected), __LINE__)

void expect_register_operands(
    const DecodedInstruction& decoded,
    RegisterIndex rd,
    RegisterIndex rs1,
    RegisterIndex rs2
) {
    EXPECT_EQ(static_cast<unsigned>(decoded.rd), static_cast<unsigned>(rd));
    EXPECT_EQ(static_cast<unsigned>(decoded.rs1), static_cast<unsigned>(rs1));
    EXPECT_EQ(static_cast<unsigned>(decoded.rs2), static_cast<unsigned>(rs2));
}

void test_r_type_instructions() {
    struct Case {
        Word funct3;
        Word funct7;
        OP op;
    };

    constexpr std::array<Case, 10> cases{{
        {0x0U, 0x00U, OP::ADD},
        {0x0U, 0x20U, OP::SUB},
        {0x1U, 0x00U, OP::SLL},
        {0x2U, 0x00U, OP::SLT},
        {0x3U, 0x00U, OP::SLTU},
        {0x4U, 0x00U, OP::XOR},
        {0x5U, 0x00U, OP::SRL},
        {0x5U, 0x20U, OP::SRA},
        {0x6U, 0x00U, OP::OR},
        {0x7U, 0x00U, OP::AND},
    }};

    for (const Case& test_case : cases) {
        const auto decoded = Decoder::decode(
            encode_r(test_case.funct7, 7U, 6U, test_case.funct3, 5U)
        );
        EXPECT_OP(decoded.op, test_case.op);
        expect_register_operands(decoded, 5U, 6U, 7U);
        EXPECT_TRUE(decoded.uses_rs1);
        EXPECT_TRUE(decoded.uses_rs2);
        EXPECT_TRUE(decoded.writes_rd);
        EXPECT_EQ(decoded.lhs_source, OperandSource::Register);
        EXPECT_EQ(decoded.rhs_source, OperandSource::Register);
    }
}

void test_i_type_arithmetic() {
    struct Case {
        Word funct3;
        OP op;
    };

    constexpr std::array<Case, 6> cases{{
        {0x0U, OP::ADDI},
        {0x2U, OP::SLTI},
        {0x3U, OP::SLTIU},
        {0x4U, OP::XORI},
        {0x6U, OP::ORI},
        {0x7U, OP::ANDI},
    }};

    for (const Case& test_case : cases) {
        const auto decoded = Decoder::decode(
            encode_i(0xff0U, 6U, test_case.funct3, 5U, kOpImmediateOpcode)
        );
        EXPECT_OP(decoded.op, test_case.op);
        expect_register_operands(decoded, 5U, 6U, 0U);
        EXPECT_EQ(decoded.immediate, 0xfffffff0U);
        EXPECT_TRUE(decoded.uses_rs1);
        EXPECT_FALSE(decoded.uses_rs2);
        EXPECT_TRUE(decoded.writes_rd);
        EXPECT_EQ(decoded.lhs_source, OperandSource::Register);
        EXPECT_EQ(decoded.rhs_source, OperandSource::Immediate);
    }

    const auto slli = Decoder::decode(
        encode_i(0x005U, 6U, 0x1U, 5U, kOpImmediateOpcode)
    );
    EXPECT_OP(slli.op, OP::SLLI);
    EXPECT_EQ(slli.immediate, 5U);

    const auto srli = Decoder::decode(
        encode_i(0x005U, 6U, 0x5U, 5U, kOpImmediateOpcode)
    );
    EXPECT_OP(srli.op, OP::SRLI);
    EXPECT_EQ(srli.immediate, 5U);

    const auto srai = Decoder::decode(
        encode_i(0x405U, 6U, 0x5U, 5U, kOpImmediateOpcode)
    );
    EXPECT_OP(srai.op, OP::SRAI);
    EXPECT_EQ(srai.immediate, 5U);
}

void test_load_store_instructions() {
    struct LoadCase {
        Word funct3;
        OP op;
    };
    constexpr std::array<LoadCase, 5> loads{{
        {0x0U, OP::LB},
        {0x1U, OP::LH},
        {0x2U, OP::LW},
        {0x4U, OP::LBU},
        {0x5U, OP::LHU},
    }};

    for (const LoadCase& test_case : loads) {
        const auto decoded = Decoder::decode(
            encode_i(0xffcU, 6U, test_case.funct3, 5U, kLoadOpcode)
        );
        EXPECT_OP(decoded.op, test_case.op);
        expect_register_operands(decoded, 5U, 6U, 0U);
        EXPECT_EQ(decoded.immediate, 0xfffffffcU);
        EXPECT_TRUE(decoded.uses_rs1);
        EXPECT_FALSE(decoded.uses_rs2);
        EXPECT_TRUE(decoded.writes_rd);
        EXPECT_TRUE(decoded.is_load);
        EXPECT_FALSE(decoded.is_store);
    }

    struct StoreCase {
        Word funct3;
        OP op;
    };
    constexpr std::array<StoreCase, 3> stores{{
        {0x0U, OP::SB},
        {0x1U, OP::SH},
        {0x2U, OP::SW},
    }};

    for (const StoreCase& test_case : stores) {
        const auto decoded = Decoder::decode(
            encode_s(0xffcU, 7U, 6U, test_case.funct3)
        );
        EXPECT_OP(decoded.op, test_case.op);
        expect_register_operands(decoded, 0U, 6U, 7U);
        EXPECT_EQ(decoded.immediate, 0xfffffffcU);
        EXPECT_TRUE(decoded.uses_rs1);
        EXPECT_TRUE(decoded.uses_rs2);
        EXPECT_FALSE(decoded.writes_rd);
        EXPECT_FALSE(decoded.is_load);
        EXPECT_TRUE(decoded.is_store);
    }
}

void test_branch_instructions() {
    struct Case {
        Word funct3;
        OP op;
    };
    constexpr std::array<Case, 6> cases{{
        {0x0U, OP::BEQ},
        {0x1U, OP::BNE},
        {0x4U, OP::BLT},
        {0x5U, OP::BGE},
        {0x6U, OP::BLTU},
        {0x7U, OP::BGEU},
    }};

    for (const Case& test_case : cases) {
        const auto decoded = Decoder::decode(
            encode_b(0x1ff0U, 7U, 6U, test_case.funct3)
        );
        EXPECT_OP(decoded.op, test_case.op);
        expect_register_operands(decoded, 0U, 6U, 7U);
        EXPECT_EQ(decoded.immediate, 0xfffffff0U);
        EXPECT_TRUE(decoded.uses_rs1);
        EXPECT_TRUE(decoded.uses_rs2);
        EXPECT_FALSE(decoded.writes_rd);
        EXPECT_TRUE(decoded.is_branch);
        EXPECT_FALSE(decoded.is_jump);
    }
}

void test_upper_and_jump_instructions() {
    const auto lui = Decoder::decode(encode_u(0xabcde000U, 5U, kLuiOpcode));
    EXPECT_OP(lui.op, OP::LUI);
    EXPECT_EQ(static_cast<unsigned>(lui.rd), 5U);
    EXPECT_EQ(lui.immediate, 0xabcde000U);
    EXPECT_FALSE(lui.uses_rs1);
    EXPECT_FALSE(lui.uses_rs2);
    EXPECT_TRUE(lui.writes_rd);
    EXPECT_EQ(lui.lhs_source, OperandSource::Zero);
    EXPECT_EQ(lui.rhs_source, OperandSource::Immediate);

    const auto auipc = Decoder::decode(encode_u(0x12345000U, 5U, kAuipcOpcode));
    EXPECT_OP(auipc.op, OP::AUIPC);
    EXPECT_EQ(auipc.immediate, 0x12345000U);
    EXPECT_EQ(auipc.lhs_source, OperandSource::ProgramCounter);
    EXPECT_EQ(auipc.rhs_source, OperandSource::Immediate);

    const auto jal = Decoder::decode(encode_j(0x1ff800U, 5U));
    EXPECT_OP(jal.op, OP::JAL);
    EXPECT_EQ(static_cast<unsigned>(jal.rd), 5U);
    EXPECT_EQ(jal.immediate, 0xfffff800U);
    EXPECT_FALSE(jal.uses_rs1);
    EXPECT_FALSE(jal.uses_rs2);
    EXPECT_TRUE(jal.writes_rd);
    EXPECT_TRUE(jal.is_jump);

    const auto jalr = Decoder::decode(
        encode_i(0xff0U, 6U, 0x0U, 5U, kJalrOpcode)
    );
    EXPECT_OP(jalr.op, OP::JALR);
    expect_register_operands(jalr, 5U, 6U, 0U);
    EXPECT_EQ(jalr.immediate, 0xfffffff0U);
    EXPECT_TRUE(jalr.uses_rs1);
    EXPECT_FALSE(jalr.uses_rs2);
    EXPECT_TRUE(jalr.writes_rd);
    EXPECT_TRUE(jalr.is_jump);
}

void test_immediate_boundaries() {
    EXPECT_EQ(
        Decoder::decode(encode_i(0x7ffU, 1U, 0x0U, 2U, kOpImmediateOpcode))
            .immediate,
        0x000007ffU
    );
    EXPECT_EQ(
        Decoder::decode(encode_i(0x800U, 1U, 0x0U, 2U, kOpImmediateOpcode))
            .immediate,
        0xfffff800U
    );

    EXPECT_EQ(Decoder::decode(encode_s(0x7ffU, 2U, 1U, 0x0U)).immediate,
              0x000007ffU);
    EXPECT_EQ(Decoder::decode(encode_s(0x800U, 2U, 1U, 0x0U)).immediate,
              0xfffff800U);

    EXPECT_EQ(Decoder::decode(encode_b(0x0ffeU, 2U, 1U, 0x0U)).immediate,
              0x00000ffeU);
    EXPECT_EQ(Decoder::decode(encode_b(0x1000U, 2U, 1U, 0x0U)).immediate,
              0xfffff000U);

    EXPECT_EQ(Decoder::decode(encode_j(0x0ffffeU, 1U)).immediate,
              0x000ffffeU);
    EXPECT_EQ(Decoder::decode(encode_j(0x100000U, 1U)).immediate,
              0xfff00000U);
}

OP expected_r_op(Word funct3, Word funct7) {
    if (funct7 == 0x00U) {
        constexpr std::array<OP, 8> base_ops{{
            OP::ADD, OP::SLL, OP::SLT, OP::SLTU,
            OP::XOR, OP::SRL, OP::OR, OP::AND,
        }};
        return base_ops[funct3];
    }
    if (funct7 == 0x20U && funct3 == 0x0U) {
        return OP::SUB;
    }
    if (funct7 == 0x20U && funct3 == 0x5U) {
        return OP::SRA;
    }
    return OP::INVALID;
}

void test_funct_exhaustion() {
    for (Word funct3 = 0; funct3 < 8U; ++funct3) {
        for (Word funct7 = 0; funct7 < 128U; ++funct7) {
            const auto decoded = Decoder::decode(
                encode_r(funct7, 2U, 1U, funct3, 3U)
            );
            EXPECT_OP(decoded.op, expected_r_op(funct3, funct7));
        }
    }

    for (Word funct7 = 0; funct7 < 128U; ++funct7) {
        const auto slli = Decoder::decode(
            encode_i(funct7 << 5U, 1U, 0x1U, 2U, kOpImmediateOpcode)
        );
        EXPECT_OP(slli.op, funct7 == 0x00U ? OP::SLLI : OP::INVALID);

        const auto right_shift = Decoder::decode(
            encode_i(funct7 << 5U, 1U, 0x5U, 2U, kOpImmediateOpcode)
        );
        const OP expected = funct7 == 0x00U
            ? OP::SRLI
            : (funct7 == 0x20U ? OP::SRAI : OP::INVALID);
        EXPECT_OP(right_shift.op, expected);
    }

    constexpr std::array<OP, 8> branch_ops{{
        OP::BEQ, OP::BNE, OP::INVALID, OP::INVALID,
        OP::BLT, OP::BGE, OP::BLTU, OP::BGEU,
    }};
    constexpr std::array<OP, 8> load_ops{{
        OP::LB, OP::LH, OP::LW, OP::INVALID,
        OP::LBU, OP::LHU, OP::INVALID, OP::INVALID,
    }};
    constexpr std::array<OP, 8> store_ops{{
        OP::SB, OP::SH, OP::SW, OP::INVALID,
        OP::INVALID, OP::INVALID, OP::INVALID, OP::INVALID,
    }};

    for (Word funct3 = 0; funct3 < 8U; ++funct3) {
        EXPECT_OP(
            Decoder::decode(encode_b(4U, 2U, 1U, funct3)).op,
            branch_ops[funct3]
        );
        EXPECT_OP(
            Decoder::decode(encode_i(4U, 1U, funct3, 2U, kLoadOpcode)).op,
            load_ops[funct3]
        );
        EXPECT_OP(
            Decoder::decode(encode_s(4U, 2U, 1U, funct3)).op,
            store_ops[funct3]
        );
        EXPECT_OP(
            Decoder::decode(encode_i(4U, 1U, funct3, 2U, kJalrOpcode)).op,
            funct3 == 0U ? OP::JALR : OP::INVALID
        );
    }
}

bool is_supported_opcode(Word opcode) {
    return opcode == kLoadOpcode
        || opcode == kOpImmediateOpcode
        || opcode == kAuipcOpcode
        || opcode == kStoreOpcode
        || opcode == kOpOpcode
        || opcode == kLuiOpcode
        || opcode == kBranchOpcode
        || opcode == kJalrOpcode
        || opcode == kJalOpcode;
}

void test_invalid_and_special_encodings() {
    EXPECT_OP(Decoder::decode(0x0ff00513U).op, OP::HALT);
    EXPECT_OP(
        Decoder::decode(encode_i(0x0feU, 0U, 0x0U, 10U, kOpImmediateOpcode)).op,
        OP::ADDI
    );

    EXPECT_OP(Decoder::decode(0x00000073U).op, OP::INVALID);
    EXPECT_OP(Decoder::decode(0x00100073U).op, OP::INVALID);
    EXPECT_OP(Decoder::decode(0x00000000U).op, OP::INVALID);
    EXPECT_OP(Decoder::decode(0xffffffffU).op, OP::INVALID);

    for (Word opcode = 0; opcode < 128U; ++opcode) {
        if (is_supported_opcode(opcode)) {
            continue;
        }
        EXPECT_OP(Decoder::decode(opcode).op, OP::INVALID);
    }

    const auto add_to_x0 = Decoder::decode(
        encode_r(0x00U, 2U, 1U, 0x0U, 0U)
    );
    EXPECT_OP(add_to_x0.op, OP::ADD);
    EXPECT_FALSE(add_to_x0.writes_rd);
}

}  // namespace

void run_decoder_tests() {
    test_r_type_instructions();
    test_i_type_arithmetic();
    test_load_store_instructions();
    test_branch_instructions();
    test_upper_and_jump_instructions();
    test_immediate_boundaries();
    test_funct_exhaustion();
    test_invalid_and_special_encodings();
}
