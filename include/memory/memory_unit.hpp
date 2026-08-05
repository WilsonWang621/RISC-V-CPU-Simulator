#pragma once

#include "common/types.hpp"
#include "memory/image.hpp"

enum class MemoryAccessType {
    LOAD,
    STORE
};

enum class MemoryWidth {
    BYTE,
    HALF,
    WORD
};

struct DataMemoryRequest{
    MemoryAccessType type = MemoryAccessType::LOAD;
    MemoryWidth width = MemoryWidth::WORD;

    Address address = 0U;

    // Store 待写入的数据；Load 时没有意义。
    Word value = 0;

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

struct InstructionFetchRequest{
    Address pc = 0;
    bool valid = false;
};

struct InstructionFetchResponse{
    Address pc = 0;
    InstructionBits instruction = 0;

    bool success = false;
    bool valid = false;
};


class MemoryUnit{
public:
    static constexpr size_t kDataLatency = 3U;

    MemoryUnit();

    void reset();
    // 清除正在处理的请求和响应，但保留内存内容
    void latch();
    
    // 装载一个新程序，同时清空所有未完成访存
    ImageLoadResult load(std::istream& input);

    InstructionFetchResponse fetch(const InstructionFetchRequest& request) const;

    // 当前数据端口能否接受新请求
    bool data_port_available() const;

    DataMemoryResponse data_response() const;

    bool evaluate(const DataMemoryRequest& request);

    void clear();

private:
    struct DataTransaction {
        DataMemoryRequest request{};
        unsigned remaining_cycles = 0U;
        bool valid = false;
    };

    struct PendingStoreWrite {
        Address address = 0;
        Word value = 0;
        MemoryWidth width = MemoryWidth::WORD;
        bool valid = false;
    };

    MemoryImage memory_{};

    DataTransaction current_transaction_{};
    DataTransaction next_transaction_{};

    DataMemoryResponse current_response_{};
    DataMemoryResponse next_response_{};

    // evaluate 阶段只准备写事件，latch 时才修改 MemoryImage
    PendingStoreWrite next_store_write_{};

    static std::size_t width_in_bytes(MemoryWidth width);

    Word read_data(Address address, MemoryWidth width, bool& success) const;

    bool schedule_store(const DataMemoryRequest& request);

    void complete_current_transaction();

    bool apply_store_write(const PendingStoreWrite& write);
};
