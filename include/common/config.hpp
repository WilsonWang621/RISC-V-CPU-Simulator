#pragma once
#include<stddef.h>
#include"types.hpp"

constexpr size_t kMemorySize = 1u << 20; // 1 MB ：  模拟内存的字节数。用于创建内存数组
constexpr size_t kInstructionWidth = 4; // 1个RV32指令 4 bytes
constexpr size_t kMemoryAddressLimit = static_cast<Address>(kMemorySize); //地址上界， 不包含该地址 合法字节地址是 [0, kMemoryAddressLimit)