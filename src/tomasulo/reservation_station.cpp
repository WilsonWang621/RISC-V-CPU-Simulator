#include"reservation_station.hpp"

ReservationStation::ReservationStation(){
    reset();
}

void ReservationStation::reset() {
    cur_rs_.fill(RSEntry{});
    next_rs_.fill(RSEntry{});

    dispatch_cursor_ = 0U;
    next_dispatch_cursor_ = 0U;
}

void ReservationStation::latch(){
    cur_rs_  = next_rs_;
    dispatch_cursor_ = next_dispatch_cursor_;
}

bool ReservationStation::empty() const{
    for(const RSEntry& entry : cur_rs_){
        if(entry.busy) return false;
    }
    return true;
}

bool ReservationStation::full() const{
    for(const RSEntry& entry : cur_rs_){
        if(!entry.busy) return false;
    }
    return true;
}

std::size_t ReservationStation::size() const {
    std::size_t count = 0U;

    for (const RSEntry& entry : cur_rs_) {
        if (entry.busy) {
            ++count;
        }
    }

    return count;
}

bool ReservationStation::available() const{
    return !full();
}

std::size_t ReservationStation::select_ready_index(){
    for(size_t offset = 0u; offset < ReservationStation::kCapacity; offset++){
        const size_t index =
            (dispatch_cursor_ + offset) % ReservationStation::kCapacity;
        const RSEntry& entry = cur_rs_[index];
        if(entry.busy && entry.lhs.ready && entry.rhs.ready){
            return index;
        } 
    }
    return ReservationStation::kInvalidIndex;
} 

Execute ReservationStation::make_execute(const RSEntry &entry){
    Execute result{};
    
    result.op = entry.op;
    result.operand1 = entry.lhs.value;
    result.operand2 = entry.rhs.value;
    result.immediate = entry.immediate;
    result.pc = entry.pc;
    result.tag = entry.destination;
    result.valid = true;

    return result;
}

void ReservationStation::wake_operand(Operand& operand, const CDBMsg& cdb){
    if(operand.ready || !operand.tag.valid || !cdb.valid || !cdb.tag.valid) return;
    // RobTag 的比较同时包含 index 和 generation，
    // 防止旧广播唤醒已经复用相同 ROB 下标的新指令。
    if (operand.tag == cdb.tag) {
        operand.value = cdb.value;
        operand.tag = RobTag{};
        operand.ready = true;
    }
}

void ReservationStation::wake_entry(RSEntry& entry, const CDBMsg& cdb){
    if(!entry.busy) return;

    wake_operand(entry.lhs, cdb);
    wake_operand(entry.rhs, cdb);
};

std::size_t ReservationStation::find_free_index(
    const std::array<RSEntry, ReservationStation::kCapacity>& entries
) {
    for (
        std::size_t index = 0U;
        index < ReservationStation::kCapacity;
        ++index
    ) {
        if (!entries[index].busy) {
            return index;
        }
    }

    return ReservationStation::kInvalidIndex;
}


RSOutputs ReservationStation::evaluate(const RSInputs& inputs){
    RSOutputs outputs{};

    // 默认保持当前完整状态
    next_dispatch_cursor_ = dispatch_cursor_;
    next_rs_ = cur_rs_;

    if(inputs.flush){
        next_dispatch_cursor_ = 0u;
        next_rs_.fill(RSEntry{});
        return outputs;
    }

    // 必须从 cur_rs_ 中选择 ready 指令。
    // 本周期被 CDB 唤醒的指令写入 next_，
    // 最早在下一周期才能被派发
    const size_t selected = select_ready_index();
    if(selected != ReservationStation::kInvalidIndex){
        outputs.dispatch = make_execute(cur_rs_[selected]);
        outputs.dispatch_valid = true;
    }
    
    //CDB唤醒所有条目
    for (RSEntry& entry : next_rs_){
        wake_entry(entry, inputs.cdb);
    }

    //只有FU真正接收，才可以释放槽位！！
    if(selected != ReservationStation::kInvalidIndex && inputs.fu_available){
        next_rs_[selected] = RSEntry{};
        next_dispatch_cursor_ =
            (selected + 1) % ReservationStation::kCapacity;
    }

    if(inputs.issue_valid){
        RSEntry new_entry = inputs.issue_entry;
         
        const size_t free_slot = find_free_index(next_rs_);

        if(free_slot != ReservationStation::kInvalidIndex){
            new_entry.busy = true;
            next_rs_[free_slot] = new_entry;
            outputs.issue_accepted = true;
        }
    }
    return outputs;
}
