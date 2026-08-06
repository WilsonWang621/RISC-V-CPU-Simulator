#pragma once

#include<array>
#include<cstddef>  

#include "common/types.hpp"
#include "isa/instruction.hpp"
#include"./signals.hpp"


// ROB 只保存执行完成、顺序提交和分支恢复所需的信息。
// RS/LSQ 中的操作数等待状态不应复制到这里。
struct ROBEntry {
    RobTag tag{};
    OP op = OP::INVALID;
    Address pc = 0;

    RegisterIndex rd = 0;
    Word value = 0;
    bool writes_rd = false;
    bool ready = false;

    Address predicted_next_pc = 0;
    Address actual_next_pc = 0;
    bool branch_taken = false;
    bool is_branch = false;
    bool is_store = false;
    bool is_jump = false;

    bool valid = false;
};

struct StoreCommitRequest { //ROB队首是store时,向LSQ发出提交请求
    RobTag tag{};
    bool valid = false;
};

struct StoreCommitCompletion { //LSQ和Unit完成真正的内存写入后返回
    RobTag tag{};
    bool valid = false;
};

struct ROBCommit{
    ROBEntry entry{};

    bool valid = false;
    bool halted = false;

    bool mispredicted = false;
    Address redirect_pc = 0;
};

struct ROBInputs{
    ROBEntry issue_entry{};
    bool issue_valid = false;

    // 获得 CDB 仲裁的执行结果。
    // 普通运算只使用 tag/result；
    // 控制指令还使用 actual_next_pc、branch_taken。
    FUResult completion{};

    // Store 真正完成内存写入的确认。
    StoreCommitCompletion store_completion{};
};


struct ROBOutputs{
    ROBCommit commit{};
    StoreCommitRequest store_request{};

    bool issue_accepted = false;

    // 分支预测失败时通知其他投机模块清空状态。
    bool flush = false;
    Address redirect_pc = 0;
};

// ROB 对 current head 的只读决策。Issue/CDB/Store completion 等本周期
// 输入只在 apply() 阶段吸收，不能反过来影响本周期提交。
struct ROBDecision {
    ROBOutputs outputs{};
};

struct ReorderBuffer{
public:
    static constexpr size_t kCapacity = 32u;

    ReorderBuffer();

    void reset();
    void latch();

    bool empty() const;
    bool full() const;
    size_t size() const;

    //Issue 前获取下一条指令要用的tag
    RobTag next_tag() const;

    const ROBEntry* front() const;
    const ROBEntry* lookup(RobTag tag) const;

    ROBDecision plan() const;
    bool apply(const ROBInputs& inputs, const ROBDecision& decision);

    const std::array<ROBEntry, kCapacity>& entries() const;

private:
    std::array<ROBEntry, kCapacity> cur_{};
    std::array<ROBEntry, kCapacity> next_{};

    // generations_[i] 表示槽位 i 最近一次分配时的 generation。
    std::array<TagGeneration, kCapacity> generations_{};
    std::array<TagGeneration, kCapacity> next_generations_{};

    std::size_t head_ = 0U;
    std::size_t tail_ = 0U;
    std::size_t count_ = 0U;

    std::size_t next_head_ = 0U;
    std::size_t next_tail_ = 0U;
    std::size_t next_count_ = 0U;

    static TagGeneration following_generation(TagGeneration generation);

    bool matching_entry(RobTag tag, const std::array<ROBEntry, kCapacity>& entries) const;

    static bool is_control_instruction(const ROBEntry& entry);

    void apply_completion(const FUResult& completion);

    void apply_store_completion(const StoreCommitCompletion& completion);

    void clear_speculative_entries(std::size_t new_postion);
};
