#include <type_traits>

#include "common/types.hpp"
#include "isa/instruction.hpp"
#include "test_support.hpp"
#include "tomasulo/load_store_queue.hpp"
#include "tomasulo/reorder_buffer.hpp"
#include "tomasulo/reservation_station.hpp"
#include "tomasulo/signals.hpp"

static_assert(sizeof(Byte) == 1U);
static_assert(sizeof(HalfWord) == 2U);
static_assert(sizeof(Word) == 4U);
static_assert(sizeof(SignedWord) == 4U);
static_assert(sizeof(Address) == 4U);
static_assert(sizeof(InstructionBits) == 4U);
static_assert(std::is_signed_v<SignedWord>);
static_assert(std::is_unsigned_v<Word>);

void run_type_tests() {
    EXPECT_EQ(bit_mask(0), 0U);
    EXPECT_EQ(bit_mask(1), 0x00000001U);
    EXPECT_EQ(bit_mask(12), 0x00000fffU);
    EXPECT_EQ(bit_mask(32), 0xffffffffU);
    EXPECT_EQ(bit_mask(33), 0xffffffffU);

    EXPECT_TRUE(get_bit(0x80000000U, 31));
    EXPECT_FALSE(get_bit(0x80000000U, 30));
    EXPECT_FALSE(get_bit(0xffffffffU, 32));

    EXPECT_EQ(extract_bits(0xdeadbeefU, 6, 0), 0x0000006fU);
    EXPECT_EQ(extract_bits(0xdeadbeefU, 31, 28), 0x0000000dU);
    EXPECT_EQ(extract_bits(0xffffffffU, 4, 5), 0U);

    EXPECT_EQ(zero_extend(0xfffffabcU, 12), 0x00000abcU);
    EXPECT_EQ(sign_extend(0x000007ffU, 12), 0x000007ffU);
    EXPECT_EQ(sign_extend(0xfffff7ffU, 12), 0x000007ffU);
    EXPECT_EQ(sign_extend(0x00000800U, 12), 0xfffff800U);
    EXPECT_EQ(sign_extend(0x00000fffU, 12), 0xffffffffU);
    EXPECT_EQ(sign_extend(0x89abcdefU, 32), 0x89abcdefU);

    const RobTag first{2U, 7U, true};
    const RobTag same{2U, 7U, true};
    const RobTag reused_slot{2U, 8U, true};
    EXPECT_TRUE(first == same);
    EXPECT_TRUE(first != reused_slot);
}
