#include <cstddef>
#include <ios>
#include <sstream>
#include <string>

#include "common/config.hpp"
#include "memory/image.hpp"
#include "test_support.hpp"

namespace {

ImageLoadResult load_text(
    MemoryImage& image,
    const std::string& text
) {
    std::istringstream input(text);
    return image.load(input);
}

bool all_bytes_are_zero(MemoryImage& image) {
    for (std::size_t address = 0; address < kMemorySize; ++address) {
        const auto value =
            image.read_byte(static_cast<Address>(address));

        if (!value || *value != Byte{0}) {
            return false;
        }
    }

    return true;
}

}  // namespace

void run_memory_tests() {
    MemoryImage image;

    // 默认构造出的整片内存均为零。
    EXPECT_TRUE(all_bytes_are_zero(image));
    EXPECT_EQ(image.loaded_byte_count(), 0U);
    EXPECT_FALSE(image.highest_loaded_address().has_value());

    // 从地址零装载，并检查装载统计。
    {
        const auto result = load_text(image, "@00000000 12 34");
        EXPECT_TRUE(result.status == ImageLoadStatus::kSuccess);
        EXPECT_EQ(result.loaded_byte_count, 2U);
        EXPECT_TRUE(image.read_byte(0).has_value());
        EXPECT_EQ(image.read_byte(0).value_or(0), Byte{0x12});
        EXPECT_EQ(image.read_byte(1).value_or(0), Byte{0x34});
        EXPECT_EQ(image.loaded_byte_count(), 2U);
        EXPECT_TRUE(image.highest_loaded_address().has_value());
        EXPECT_EQ(image.highest_loaded_address().value_or(0), 1U);
    }

    // 从非零地址装载。
    {
        const auto result = load_text(image, "@00000100 ab cd");
        EXPECT_TRUE(result.status == ImageLoadStatus::kSuccess);
        EXPECT_EQ(image.read_byte(0x100).value_or(0), Byte{0xab});
        EXPECT_EQ(image.read_byte(0x101).value_or(0), Byte{0xcd});
        EXPECT_EQ(image.read_byte(0).value_or(1), Byte{0});
    }

    // 多个 @地址可以形成不连续的装载区域，中间仍保持为零。
    {
        const auto result =
            load_text(image, "@00000010 aa bb @00000020 cc");
        EXPECT_TRUE(result.status == ImageLoadStatus::kSuccess);
        EXPECT_EQ(result.loaded_byte_count, 3U);
        EXPECT_EQ(image.read_byte(0x10).value_or(0), Byte{0xaa});
        EXPECT_EQ(image.read_byte(0x11).value_or(0), Byte{0xbb});
        EXPECT_EQ(image.read_byte(0x20).value_or(0), Byte{0xcc});
        EXPECT_EQ(image.read_byte(0x12).value_or(1), Byte{0});
        EXPECT_EQ(image.read_byte(0x1f).value_or(1), Byte{0});
        EXPECT_EQ(image.highest_loaded_address().value_or(0), 0x20U);
    }

    // 小端序 word 和 half 读取。
    {
        const auto result =
            load_text(image, "@00001000 37 01 02 00");
        EXPECT_TRUE(result.status == ImageLoadStatus::kSuccess);
        EXPECT_EQ(
            image.read_word(0x1000).value_or(0),
            Word{0x00020137}
        );
        EXPECT_EQ(
            image.read_half(0x1000).value_or(0),
            HalfWord{0x0137}
        );
        EXPECT_EQ(
            image.read_half(0x1002).value_or(0),
            HalfWord{0x0002}
        );
    }

    // half 和 word 写入同样采用小端序。
    image.clear();
    EXPECT_TRUE(image.write_half(0x20, HalfWord{0xabcd}));
    EXPECT_EQ(image.read_byte(0x20).value_or(0), Byte{0xcd});
    EXPECT_EQ(image.read_byte(0x21).value_or(0), Byte{0xab});
    EXPECT_EQ(image.read_half(0x20).value_or(0), HalfWord{0xabcd});

    EXPECT_TRUE(image.write_word(0x30, Word{0x89abcdef}));
    EXPECT_EQ(image.read_byte(0x30).value_or(0), Byte{0xef});
    EXPECT_EQ(image.read_byte(0x31).value_or(0), Byte{0xcd});
    EXPECT_EQ(image.read_byte(0x32).value_or(0), Byte{0xab});
    EXPECT_EQ(image.read_byte(0x33).value_or(0), Byte{0x89});
    EXPECT_EQ(image.read_word(0x30).value_or(0), Word{0x89abcdef});

    // 地址和字节都接受大小写十六进制字符。
    {
        const auto result = load_text(image, "@0000000A aF Bc");
        EXPECT_TRUE(result.status == ImageLoadStatus::kSuccess);
        EXPECT_EQ(image.read_byte(0x0a).value_or(0), Byte{0xaf});
        EXPECT_EQ(image.read_byte(0x0b).value_or(0), Byte{0xbc});
    }

    // 支持多行、空行、制表符及多余空白。
    {
        const auto result = load_text(
            image,
            "\n  @00000004\t01  02 \n\n 03\t04   \n"
        );
        EXPECT_TRUE(result.status == ImageLoadStatus::kSuccess);
        EXPECT_EQ(result.loaded_byte_count, 4U);
        EXPECT_EQ(image.read_word(4).value_or(0), Word{0x04030201});
    }

    // 数据前没有 @地址。
    {
        const auto result = load_text(image, "\n  37 01");
        EXPECT_TRUE(
            result.status == ImageLoadStatus::kMissingAddress
        );
        EXPECT_EQ(result.error_line, 2U);
        EXPECT_TRUE(result.error_token == "37");
        EXPECT_EQ(result.loaded_byte_count, 0U);
    }

    // 非法地址 token。
    {
        const auto result = load_text(image, "@not_hex 01");
        EXPECT_TRUE(
            result.status == ImageLoadStatus::kInvalidAddress
        );
        EXPECT_EQ(result.error_line, 1U);
        EXPECT_TRUE(result.error_token == "@not_hex");
    }

    // 非法字节 token。
    {
        const auto result = load_text(image, "@0 12 gg");
        EXPECT_TRUE(result.status == ImageLoadStatus::kInvalidByte);
        EXPECT_EQ(result.loaded_byte_count, 1U);
        EXPECT_TRUE(result.error_token == "gg");
    }

    // 十六进制数合法，但超过一个字节的取值范围。
    {
        const auto result = load_text(image, "@0 100");
        EXPECT_TRUE(result.status == ImageLoadStatus::kInvalidByte);
        EXPECT_EQ(result.loaded_byte_count, 0U);
        EXPECT_TRUE(result.error_token == "100");
    }

    // @地址本身必须落在模拟内存内。
    {
        const auto result = load_text(image, "@100000 01");
        EXPECT_TRUE(
            result.status == ImageLoadStatus::kAddressOutOfBounds
        );
        EXPECT_TRUE(result.error_token == "@100000");
    }

    // 最后一个地址允许单字节读写。
    image.clear();
    constexpr Address last_address =
        static_cast<Address>(kMemorySize - 1U);
    EXPECT_TRUE(image.write_byte(last_address, Byte{0x5a}));
    EXPECT_EQ(image.read_byte(last_address).value_or(0), Byte{0x5a});
    EXPECT_FALSE(
        image.read_byte(static_cast<Address>(kMemorySize)).has_value()
    );

    // half 和 word 必须检查完整访问范围。
    EXPECT_FALSE(image.read_half(last_address).has_value());
    EXPECT_FALSE(image.write_half(last_address, HalfWord{0x1234}));
    EXPECT_FALSE(
        image.read_word(
            static_cast<Address>(kMemorySize - 2U)
        ).has_value()
    );
    EXPECT_FALSE(
        image.write_word(
            static_cast<Address>(kMemorySize - 3U),
            Word{0x12345678}
        )
    );

    // 在最后一个地址装载一字节后，地址递增至边界；
    // 再出现一个数据字节时必须报告越界。
    {
        const auto result = load_text(image, "@fffff aa bb");
        EXPECT_TRUE(
            result.status == ImageLoadStatus::kAddressOutOfBounds
        );
        EXPECT_EQ(result.loaded_byte_count, 1U);
        EXPECT_TRUE(result.error_token == "bb");
        EXPECT_EQ(image.read_byte(last_address).value_or(0), Byte{0xaa});
        EXPECT_EQ(image.loaded_byte_count(), 1U);
        EXPECT_EQ(
            image.highest_loaded_address().value_or(0),
            last_address
        );
    }

    // 底层输入流错误。
    {
        std::istringstream input("@0 01");
        input.setstate(std::ios::badbit);
        const auto result = image.load(input);
        EXPECT_TRUE(result.status == ImageLoadStatus::kStreamError);
        EXPECT_EQ(result.loaded_byte_count, 0U);
    }

    // clear 同时清除内容和装载元数据。
    EXPECT_TRUE(image.write_word(0x40, Word{0xffffffff}));
    image.clear();
    EXPECT_TRUE(all_bytes_are_zero(image));
    EXPECT_EQ(image.loaded_byte_count(), 0U);
    EXPECT_FALSE(image.highest_loaded_address().has_value());
}
