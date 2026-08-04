#pragma once

#include<array>
#include<cstddef>

#include "common/types.hpp"
#include "isa/instruction.hpp"
#include "tomasulo/signals.hpp"
#include"memory/memory_unit.hpp"
#include"tomasulo/reorder_buffer.hpp"

// address == 0 是合法地址，因此必须使用 address_ready 单独表示地址是否就绪。
// Store 的数据只有到达 ROB 队首后才允许通过 Memory Unit 修改内存。
struct LSQEntry {
    MemoryAccessType type = MemoryAccessType::LOAD;
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

struct LSQInputs{
    LSQEntry issue_entry{};
    bool issue_valid = false;
    
    CDBMsg cdb{};

    // ROB 队首 Store 的写内存授权。
    StoreCommitRequest store_request{};

    // Memory Unit 延迟返回的响应。
    DataMemoryResponse memory_response{};

    // Memory Unit 本周期能否接受数据请求。
    bool memory_available = false;

    // 当前 Load 结果是否获得 CDB 仲裁。
    bool load_result_granted = false;

    // 分支错误预测恢复。
    bool flush = false;
};

struct LSQOutputs {
    DataMemoryRequest memory_request{};

    // Store 真正写入内存完成后返回给 ROB。
    StoreCommitCompletion store_completion{};

    bool issue_accepted = false;
    bool memory_error = false;
};


class LoadStoreQueue{
public:
    static constexpr size_t kCapacity = 16U;

    LoadStoreQueue();

    void reset();
    void latch();

    bool empty() const;
    bool full() const;
    size_t size() const;

    // 当前已经完成、等待 CDB 的 Load 结果。
    FUResult load_result() const;

    LSQOutputs evaluate(const LSQInputs& inputs);

    const std::array<LSQEntry, LoadStoreQueue::kCapacity>& entries() const;
private:
    std::array<LSQEntry, kCapacity> cur_{};
    std::array<LSQEntry, kCapacity> next_{};
    
    std::size_t head_ = 0U;
    std::size_t tail_ = 0U;
    std::size_t count_ = 0U;

    std::size_t next_head_ = 0U;
    std::size_t next_tail_ = 0U;
    std::size_t next_count_ = 0U;

    //Load 的内存响应可能无法立即获得CDB， 必须在LSQ中保存
    FUResult cur_load_result_{};
    FUResult next_load_result_{};

    void wake_operand(Operand& operand, const CDBMsg& cdb);
    void wake_entry(LSQEntry& entry, const CDBMsg& cdb);

    bool matching_response(const LSQEntry& entry, const DataMemoryResponse& response);

    static MemoryWidth memory_width(OP op);
    Word extend_load_result(OP op, Word raw_value);

    void update_addresses();

    void remove_next_head();

    static DataMemoryRequest make_memory_request(const LSQEntry& entry);
};
