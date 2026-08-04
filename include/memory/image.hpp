#pragma once
#include"../common/types.hpp"
#include"../common/config.hpp"
#include<array>
#include<stddef.h>
#include<optional>
#include<string>
#include<istream>

enum class ImageLoadStatus {
    kSuccess,
    kInvalidAddress,
    kInvalidByte,
    kMissingAddress,
    kAddressOutOfBounds,
    kStreamError,
};

struct ImageLoadResult {
    ImageLoadStatus status = ImageLoadStatus::kSuccess;
    std::size_t loaded_byte_count = 0;
    std::size_t error_line = 0;
    std::string error_token;

    bool ok() const{
        return status == ImageLoadStatus::kSuccess;
    }
};

class MemoryImage{
    std::array<Byte, kMemoryAddressLimit> bytes_{};
    size_t load_byte_count_ = 0;
    // optional 可以区分“最高地址为 0”和“尚未装载任何字节”(std::nullopt)
    std::optional<Address> highest_loaded_address_;

public:
    MemoryImage() = default;

    void clear();
    ImageLoadResult load(std::istream& input);
    bool in_bounds(Address address, size_t width);

    std::optional<Byte> read_byte(Address address);
    std::optional<HalfWord> read_half(Address address);
    std::optional<Word> read_word(Address address);

    bool write_byte(Address address, Byte value);
    bool write_half(Address address, HalfWord value);
    bool write_word(Address address, Word value);

    size_t loaded_byte_count();
    std::optional<Address> highest_loaded_address();
};