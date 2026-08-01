#include"reorder_buffer.hpp"


ReorderBuffer::ReorderBuffer(){
    reset();
}

void ReorderBuffer::reset(){
    cur_.fill(ROBEntry{});
    next_.fill(ROBEntry{});
    generations_.fill(TagGeneration{0});
    next_generations_.fill(TagGeneration{0});

    head_ = tail_ = count_ = 0U;
    next_head_ = next_tail_ = next_count_ = 0U;
}

void ReorderBuffer::latch(){
    cur_ = next_;
    generations_ = next_generations_;
    head_ = next_head_;
    tail_ = next_tail_;
    count_ = next_count_;
}

bool ReorderBuffer::empty() const{
    return count_ == 0U;
}

bool ReorderBuffer::full() const{
    return count_ == ReorderBuffer::kCapacity;
}

bool ReorderBuffer::size() const{
    return count_;
}
TagGeneration ReorderBuffer::following_generation(TagGeneration generation){
    TagGeneration next = generation + TagGeneration{1};
    return next;
}

bool ReorderBuffer::matching_entry(
    RobTag tag,
    const std::array<ROBEntry, ReorderBuffer::kCapacity>& entries
) const{
    if(!tag.valid) return false;

    const size_t index = static_cast<size_t>(tag.index);

    if(index > ReorderBuffer::kCapacity) return false;

    return entries[index].valid && entries[index].tag == tag;
}

RobTag ReorderBuffer::next_tag() const{
    if(full()){
        return RobTag{};
    }
    RobTag tag{};
    tag.valid = true;
    tag.index = static_cast<RobIndex>(tail_);
    tag.generation = following_generation(generations_[tail_]);

    return tag;
}

const ROBEntry* ReorderBuffer::front() const{
    if(empty()) return nullptr;

    const ROBEntry& entry = cur_[head_];
    //槽位里有还不一定有效
    return entry.valid ? &entry : nullptr;
}

const ROBEntry* ReorderBuffer::lookup(RobTag tag) const{
    if(!matching_entry(tag, cur_)) return nullptr;

    return &cur_[static_cast<size_t>(tag.index)];

}

ROBOutputs ReorderBuffer::evaluate(const ROBInputs &inputs){
    ROBOutputs outputs{};

    next_ = cur_;
    next_generations_ = generations_;

    next_head_ = head_;
    next_tail_ = tail_;
    next_count_ = count_;

    // 本周期到达的执行结果写入 next_。
    // 因此结果到达的指令最早下一周期提交。
    apply_completion(inputs.completion);
    apply_store_completion(inputs.store_completion);

    const ROBEntry* head_entry = front();
    //ROB 队首是一个尚未完成内存写入的 Store，通知 LSQ：这条 Store 已经获得顺序提交授权，可以尝试写内存
    if(head_entry != nullptr && head_entry->is_store && !head_entry->ready){
        outputs.store_request.valid = true;
        outputs.store_request.tag = head_entry->tag;
    }

    bool commit_caused_flush = false;

    if(head_entry != nullptr && head_entry->ready){
        outputs.commit.entry = *head_entry;
        outputs.commit.valid = true;
        outputs.commit.halted = head_entry->op == OP::HALT;

        if(is_control_instruction(*head_entry)){
            outputs.commit.mispredicted = head_entry->predicted_next_pc != head_entry->actual_next_pc;
            outputs.commit.redirect_pc = head_entry->actual_next_pc;
        }

        const size_t old_head = next_head_;
        const size_t new_head =
            (old_head + 1U) % ReorderBuffer::kCapacity;

        next_[old_head] = ROBEntry{};
        next_head_ = new_head;
        next_count_--;

        if (outputs.commit.mispredicted) {
            outputs.flush = true;
            outputs.redirect_pc = head_entry->actual_next_pc;

            commit_caused_flush = true;

            // 控制指令本身已经提交，其余全部是年轻指令。
            clear_speculative_entries(new_head);
        }
    }

    // Flush 优先级高于同周期 Issue。
    // Issue Unit 即使已经生成了 entry，也必须整体作废
    if (!commit_caused_flush && inputs.issue_valid) {
        // 资源判断使用 current_，保持模块时序一致。
        // 当前周期 ROB 是满的，即使同时提交一项，也暂时不允许发射（先完成保守但简单的实现）
        if (!full()) {
            const RobTag expected_tag = next_tag();

            if (inputs.issue_entry.valid && inputs.issue_entry.tag == expected_tag) {
                ROBEntry entry = inputs.issue_entry;

                entry.tag = expected_tag;
                entry.valid = true;

                next_[next_tail_] = entry;
                next_generations_[next_tail_] = expected_tag.generation;
                next_tail_ =
                    (next_tail_ + 1U) % ReorderBuffer::kCapacity;

                ++next_count_;
                outputs.issue_accepted = true;
            }
        }
    }

    return outputs;
}

const std::array<ROBEntry, ReorderBuffer::kCapacity>& ReorderBuffer::entries() const{
    return cur_;
}

bool ReorderBuffer::is_control_instruction(const ROBEntry& entry){
    return entry.is_jump || entry.is_branch;
}

void ReorderBuffer::apply_completion(const FUResult& completion){
    if (!completion.valid || !matching_entry(completion.tag, cur_)) {
        return;
    }

    const std::size_t index = static_cast<std::size_t>(completion.tag.index);

    ROBEntry& entry = next_[index];

    entry.value = completion.result;
    entry.ready = true;

    if (completion.has_control_result) {
        entry.actual_next_pc = completion.actual_next_pc;
        entry.branch_taken = completion.branch_taken;
    }
}

void ReorderBuffer::apply_store_completion(const StoreCommitCompletion& completion){
    if (!completion.valid || !matching_entry(completion.tag, cur_)) {
        return;
    }

    const std::size_t index = static_cast<std::size_t>(completion.tag.index);

    ROBEntry& entry = next_[index];
    if (!entry.is_store) {
        return;
    }

    entry.ready = true;
}

// generation 不能清空
// 错误路径上的旧结果可能延迟到达，保留 generation 防止这些旧结果命中未来复用相同槽位的新指令
void ReorderBuffer::clear_speculative_entries(size_t new_position){
    next_.fill(ROBEntry{});

    next_head_ = new_position;
    next_tail_ = new_position;
    next_count_ = 0U;
}
