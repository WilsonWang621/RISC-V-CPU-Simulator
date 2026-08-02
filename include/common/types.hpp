#pragma once

#include <cstdint>
//trick : 这样如果发现位数定义错了，只需要修改这一处
using Byte = std::uint8_t;
using HalfWord = std::uint16_t;
using Word = std::uint32_t;
using SignedWord = std::int32_t;
using Address = std::uint32_t;
using InstructionBits = std::uint32_t;
using RegisterIndex = std::uint8_t;
using CycleCount = std::uint64_t;
using RobIndex = std::uint32_t;
using TagGeneration = std::uint32_t;

// ROB 槽位会循环复用。index 定位槽位，generation 区分同一槽位的新旧指令（第几次被使用）
// 防止延迟到达的旧广播误命中新指令。
struct RobTag {
    RobIndex index = 0;
    TagGeneration generation = 0;
    bool valid = false;
};

constexpr bool operator==(RobTag lhs, RobTag rhs) noexcept {
    return lhs.index == rhs.index
        && lhs.generation == rhs.generation
        && lhs.valid == rhs.valid;
}

constexpr bool operator!=(RobTag lhs, RobTag rhs) noexcept {
    return !(lhs == rhs);
}

// bit_width == 0 时掩码为 0；bit_width >= 32 时掩码覆盖整个 Word。
constexpr Word bit_mask(unsigned bit_width) noexcept {
    if (bit_width == 0U) {
        return 0U;
    }
    if (bit_width >= 32U) {
        return UINT32_MAX;
    }
    return (Word{1} << bit_width) - Word{1};
}

// 越界 bit 视为 0，避免发生位移宽度大于等于 32 的未定义行为。
constexpr bool get_bit(Word value, unsigned position) noexcept {
    return position < 32U && ((value >> position) & Word{1}) != 0U;
}

// high/low 均为闭区间。非法区间返回 0，high 超过 31 时截到 31。
constexpr Word extract_bits(Word value, unsigned high, unsigned low) noexcept {
    if (low >= 32U || high < low) {
        return 0U;
    }
    const unsigned bounded_high = high < 32U ? high : 31U;
    const unsigned width = bounded_high - low + 1U;
    return (value >> low) & bit_mask(width);
}

// 按bit_width位的值拓展到最高位
constexpr Word sign_extend(Word value, unsigned bit_width) noexcept {
    if (bit_width == 0U) {
        return 0U;
    }
    if (bit_width >= 32U) {
        return value;
    }

    const Word narrowed = value & bit_mask(bit_width);
    const Word sign_bit = Word{1} << (bit_width - 1U);
    return (narrowed & sign_bit) != 0U ? narrowed | ~bit_mask(bit_width) : narrowed;
}
//bit_width以上高位拓展0
constexpr Word zero_extend(Word value, unsigned bit_width) noexcept {
    return value & bit_mask(bit_width);
}
