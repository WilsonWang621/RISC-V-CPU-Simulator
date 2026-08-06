#include"load_store_queue.hpp"

#include<cassert>

LoadStoreQueue::LoadStoreQueue(){
    reset();
}

void LoadStoreQueue::reset(){
    cur_.fill(LSQEntry{});
    next_.fill(LSQEntry{});
    head_ = tail_ = count_ = 0U;
    next_head_ = next_tail_ = next_count_ = 0U;
    cur_load_result_ = FUResult{};
    next_load_result_ = FUResult{};
}

void LoadStoreQueue::latch(){
    cur_ = next_;
    head_ = next_head_;
    tail_ = next_tail_;
    count_ = next_count_;
    cur_load_result_ = next_load_result_;
}

bool LoadStoreQueue::empty()const{
    return count_ == 0U;
}

bool LoadStoreQueue::full() const{
    return count_ == kCapacity;
}

size_t LoadStoreQueue::size() const{
    return count_;
}

FUResult LoadStoreQueue::load_result() const{
    return cur_load_result_;
}

const std::array<LSQEntry, LoadStoreQueue::kCapacity>& LoadStoreQueue::entries()const{
    return cur_;
}

void LoadStoreQueue::wake_operand(Operand& operand, const CDBMsg& cdb){
    if(operand.ready || !cdb.valid || !cdb.tag.valid || !operand.tag.valid) return;
    if(operand.tag == cdb.tag){
        operand.value = cdb.value;
        operand.ready = true;
        operand.tag = RobTag{};
    }
}
void LoadStoreQueue::wake_entry(LSQEntry& entry, const CDBMsg& cdb){
    if(!entry.valid) return;

    wake_operand(entry.base, cdb);

    if(entry.type == MemoryAccessType::STORE){
        wake_operand(entry.store_data, cdb);
    }
}

//暂时只实现只有 LSQ 队首可能有未完成请求
bool LoadStoreQueue::matching_response(const LSQEntry& entry, const DataMemoryResponse& response) const{
    if(!entry.valid || !response.valid || !response.tag.valid) return false;

    if (entry.destination != response.tag) {
        return false;
    }

    const bool entry_is_store = entry.type == MemoryAccessType::STORE;

    return entry_is_store == response.is_store;
}

MemoryWidth LoadStoreQueue::memory_width(OP op){
    switch (op){
        case OP::LB:
        case OP::LBU:
        case OP::SB:
            return MemoryWidth::BYTE;

        case OP::LH:
        case OP::LHU:
        case OP::SH:
            return MemoryWidth::HALF;

        case OP::LW:
        case OP::SW:
            return MemoryWidth::WORD;

        default:
            assert(false);
            return MemoryWidth::WORD;
    }
}
Word LoadStoreQueue::extend_load_result(OP op, Word raw_value){
    switch (op){
        case OP::LB:
            return sign_extend(raw_value, 8U);
        case OP::LH:
            return sign_extend(raw_value, 16U);

        case OP::LW:
            return raw_value;

        case OP::LBU:
            return zero_extend(raw_value, 8U);

        case OP::LHU:
            return zero_extend(raw_value, 16U);

        default:
            assert(false);
            return Word{0};
    }
}

void LoadStoreQueue::update_addresses(){
    for (LSQEntry& entry : next_) {
        if (!entry.valid || entry.address_ready || !entry.base.ready) {
            continue;
        }

        entry.address = static_cast<Address>(entry.base.value + entry.immediate);

        entry.address_ready = true;
    }
}

void LoadStoreQueue::remove_next_head(){
    assert(next_count_ > 0U);

    next_[next_head_] = LSQEntry{};
    next_head_ = (next_head_ + 1U) % kCapacity;

    --next_count_;
}

DataMemoryRequest LoadStoreQueue::make_memory_request(const LSQEntry& entry){
    DataMemoryRequest request;
    request.type = entry.type;
    request.width = memory_width(entry.op);
    request.address = entry.address;
    request.tag = entry.destination;
    request.valid = true;
    if(entry.type == MemoryAccessType::STORE){
        request.value = entry.store_data.value;
    }
    return request;
}

LSQDecision LoadStoreQueue::plan(const LSQObserveInputs& inputs) const{
    LSQDecision decision{};
    LSQOutputs& output = decision.outputs;

    if(inputs.flush){
        return decision;
    }

    decision.clear_load_result = inputs.load_result_granted && cur_load_result_.valid;

    /*
    * 处理 Memory Unit 响应。
    * 响应只匹配 current_ 中已经发送请求的队首项。
    * 这样本周期新发出的请求不可能在同周期立即完成。
    */
    if(!empty()){
        const LSQEntry& current_head = cur_[head_];
        if(matching_response(current_head, inputs.memory_response)){
            if(!inputs.memory_response.success){
                output.memory_error = true;
            }
            else if(current_head.type == MemoryAccessType::LOAD){
                FUResult result;
                result.tag = current_head.destination;
                result.result = extend_load_result(current_head.op, inputs.memory_response.value);
                result.valid = true;

                decision.completed_load_result = result;
                decision.install_load_result = true;
                decision.remove_head = true;
            }
            else{
                //Store已经真正写入内存
                output.store_completion.tag = current_head.destination;
                output.store_completion.valid = true;
                decision.remove_head = true;
            }
        }
    }

    /*
    * 从 current_ 的队首决定是否发送新请求。
    * 当前实现严格按 LSQ 顺序访存，因此天然不会让 Load
    * 越过任何更老 Store。
    */
    if(!empty()){
        const LSQEntry& current_head = cur_[head_];
        if(
            current_head.valid
            && !current_head.request_sent
            && current_head.address_ready
            && inputs.memory_available
        ){
            bool may_send = false;
            if(current_head.type == MemoryAccessType::LOAD){
                //Load结果缓冲必须可用
                may_send = !cur_load_result_.valid || inputs.load_result_granted;
            }else{
                may_send = current_head.store_data.ready && inputs.store_request.valid && inputs.store_request.tag == current_head.destination;
            }
            if(may_send){
                output.memory_request = make_memory_request(current_head);
                decision.mark_request_sent = true;
            }
        }
    }

    return decision;
}

bool LoadStoreQueue::apply(
    const LSQInputs& inputs,
    const LSQDecision& decision
){
    next_ = cur_;
    next_head_ = head_;
    next_tail_ = tail_;
    next_count_ = count_;
    next_load_result_ = cur_load_result_;


    if(inputs.flush){
        next_.fill(LSQEntry{});

        next_head_ = next_tail_ = next_count_ = 0U;
        next_load_result_ = FUResult{};

        return false;
    }

    // 当前 Load 结果获得 CDB 仲裁，可以释放结果缓冲
    if(decision.clear_load_result){
        next_load_result_ = FUResult{};
    }

    for(LSQEntry& entry : next_){
        wake_entry(entry, inputs.cdb);
    }

    if(decision.install_load_result){
        // 旧结果若在本周期获得 grant，已经先被清除。
        assert(!next_load_result_.valid);
        next_load_result_ = decision.completed_load_result;
    }

    if(decision.remove_head){
        remove_next_head();
    }

    if(decision.mark_request_sent){
        next_[head_].request_sent = true;
    }

    /*
    * 接收 Issue 的新条目。
    * available() 使用 current_，所以当前满时，即使本周期
    * 正好移除队首，也保守地不接受新条目。
    */
    bool issue_accepted = false;
    if (inputs.issue_valid && !full() && inputs.issue_entry.valid) {
        LSQEntry entry = inputs.issue_entry;

        entry.valid = true;
        entry.request_sent = false;

        next_[next_tail_] = entry;
        next_tail_ = (next_tail_ + 1U) % kCapacity;

        ++next_count_;
        issue_accepted = true;
    }
    // 为 CDB 唤醒或本周期新插入的条目计算地址。
    update_addresses();

    return issue_accepted;
}
