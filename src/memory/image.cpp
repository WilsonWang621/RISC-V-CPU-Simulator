#include"image.hpp"

#include <algorithm>
#include <charconv>
#include <cstdint>
#include <sstream>
#include <string_view>
#include <system_error>

namespace {

bool parse_hex(std::string_view text, std::uint64_t& result) noexcept { //把十六进制字符串解析为无符号64位整数
    if (text.empty()) {
        return false;
    }

    result = 0;  

    const char* const begin = text.data();
    const char* const end = begin + text.size();

    const auto [position, error] = std::from_chars(begin, end, result, 16); //高性能字符串转换函数：从 begin 到 end 的字符按照16进制转换成整数保存到 result

    return error == std::errc{} && position == end;
}

}

void MemoryImage::clear(){
    bytes_.fill(Byte{0});
    load_byte_count_ = 0;
    highest_loaded_address_.reset(); //optional 内置重置函数
}

ImageLoadResult MemoryImage::load(std::istream& input){
    clear();
    std::size_t current_address = 0;
    std::string token;
    std::size_t line_number = 0;
    std::size_t bytes_loaded = 0;
    bool has_current_address = false;
    auto make_result = [&](ImageLoadStatus status, std::size_t error_line = 0, std::string error_token = {}) {
        return ImageLoadResult{
            status,
            bytes_loaded,
            error_line,
            std::move(error_token),
        };
    };
    std::string line;

    while(std::getline(input, line)){
        line_number++;
        std::istringstream line_stream(line);
        while(line_stream >> token){
            if(!token.empty() && token.front() == '@'){
                std::uint64_t parsed_address = 0;
                const std::string_view address_text{
                    token.data() + 1,
                    token.size() - 1
                };

                if (!parse_hex(address_text, parsed_address)) {
                    return make_result(
                        ImageLoadStatus::kInvalidAddress,
                        line_number,
                        token
                    );
                }
            
                if (parsed_address >= kMemorySize) {
                    return make_result(
                        ImageLoadStatus::kAddressOutOfBounds,
                        line_number,
                        token
                    );
                }

                current_address = static_cast<size_t>(parsed_address);
                has_current_address = true;
                continue;
            }
            if (!has_current_address) {
                return make_result(
                    ImageLoadStatus::kMissingAddress,
                    line_number,
                    token
                );
            }
            std::uint64_t parsed_byte = 0;

            if (!parse_hex(token, parsed_byte) ||
                parsed_byte > 0xffU) {
                return make_result(
                    ImageLoadStatus::kInvalidByte,
                    line_number,
                    token
                );
            }

            if (current_address >= kMemorySize) {
                return make_result(
                    ImageLoadStatus::kAddressOutOfBounds,
                    line_number,
                    token
                );
            }

            bytes_[current_address] = static_cast<Byte>(parsed_byte);
            ++bytes_loaded;
            load_byte_count_ = bytes_loaded;

            const Address loaded_address = static_cast<Address>(current_address);

            if(!highest_loaded_address_ || loaded_address > *highest_loaded_address_){
                highest_loaded_address_ = loaded_address;
            }

            current_address++;
        }
    }
if(input.bad()){
    return make_result(ImageLoadStatus::kStreamError);
}

return make_result(ImageLoadStatus::kSuccess);
}

bool MemoryImage::in_bounds(Address address, size_t width){
    const size_t start = static_cast<size_t>(address);

    if(width > kMemorySize) return false;

    return start <= kMemorySize - width;
}

std::optional<Byte> MemoryImage::read_byte(Address address){
    if(!in_bounds(address, sizeof(Byte))){
        return std::nullopt;
    }
    return bytes_[static_cast<size_t>(address)];
}

std::optional<HalfWord> MemoryImage::read_half(Address address){
    if(!in_bounds(address, sizeof(HalfWord))){
        return std::nullopt;
    }
    const size_t start = static_cast<size_t>(address);
    const HalfWord low = static_cast<HalfWord>(bytes_[start]);
    const HalfWord high = static_cast<HalfWord>(bytes_[start + 1]);

    return static_cast<HalfWord>(low | static_cast<HalfWord>(high << 8U));
}

std::optional<Word> MemoryImage::read_word(Address address){
    if (!in_bounds(address, sizeof(Word))) {
        return std::nullopt;
    }    

    const size_t start = static_cast<size_t>(address);
    return static_cast<Word>(
        static_cast<Word>(bytes_[start]) |
        (static_cast<Word>(bytes_[start + 1]) << 8U) |
        (static_cast<Word>(bytes_[start + 2]) << 16U) |
        (static_cast<Word>(bytes_[start + 3]) << 24U)
    );
}


bool MemoryImage::write_byte(Address address, Byte value){
    if(!in_bounds(address, sizeof(Byte))) return false;
    bytes_[static_cast<size_t>(address)] = value;
    return true;
}

bool MemoryImage::write_half(Address address, HalfWord value){
    if (!in_bounds(address, sizeof(HalfWord))) {
        return false;
    }

    const size_t start = static_cast<size_t>(address);
    bytes_[start] = static_cast<size_t>(value & 0xffU);
    bytes_[start + 1] = static_cast<size_t>((value >> 8u) & 0xffU);

    return true;
}

bool MemoryImage::write_word(Address address, Word value){
    if (!in_bounds(address, sizeof(Word))) {
        return false;
    }

    const size_t start = static_cast<size_t>(address);

    bytes_[start] = static_cast<Byte>(value & 0xffU);

    bytes_[start + 1] = static_cast<Byte>((value >> 8U) & 0xffU);

    bytes_[start + 2] = static_cast<Byte>((value >> 16U) & 0xffU);

    bytes_[start + 3] = static_cast<Byte>((value >> 24U) & 0xffU);

    return true;
}

std::size_t MemoryImage::loaded_byte_count() {
    return load_byte_count_;
}

std::optional<Address> MemoryImage::highest_loaded_address(){
    return highest_loaded_address_;
}
