#pragma once

#include "common/types.hpp"
#include "isa/instruction.hpp"
#include "tomasulo/signals.hpp"

enum class LSType {
    LOAD,
    STORE
};

// address == 0 是合法地址，因此必须使用 address_ready 单独表示地址是否就绪。
// Store 的数据只有到达 ROB 队首后才允许通过 Memory Unit 修改内存。
struct LSQEntry {
    LSType type = LSType::LOAD;
    OP op = OP::INVALID;
    RobTag destination{};

    Operand base{};
    Operand store_data{};
    Word immediate = 0;

    Address address = 0;
    bool address_ready = false;
    bool request_sent = false;
    bool valid = false;
};
