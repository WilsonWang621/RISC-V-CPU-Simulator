#pragma once

enum class MemoryWidth {
    BYTE,
    HALF,
    WORD
};

struct DataMemoryRequest{
    LSType type = LSType::LOAD;
    MemoryWidth width = MemoryWidth::WORD;

    Address address = 0U;
    Word value = 0U;

    RobTag tag{};
    bool valid = false;
};

struct DataMemoryResponse{
    RobTag tag{};

    // Load：从内存读取的未扩展原始数据。
    // Store：该字段无意义。
    Word value = 0U;
     
    bool is_store = false;
    bool success = false;
    bool valid = false;
};
