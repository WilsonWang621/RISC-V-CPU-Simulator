#include"rename_table.hpp"

RenameTable::RenameTable(){
    reset();
}

void RenameTable::reset(){
    invalidate(cur_table_);
    invalidate(next_table_);
}

void RenameTable::invalidate(std::array<RobTag, kRegisterCount>& table){
    table.fill(RobTag{});
}

RobTag RenameTable::lookup(RegisterIndex idx){
    if(!valid_register(idx) || !cur_table_[static_cast<std::size_t>(idx)].valid){
        return RobTag{};
    }
    return cur_table_[static_cast<std::size_t>(idx)];
}

bool RenameTable::valid_register(RegisterIndex reg){
    const std::size_t index = static_cast<std::size_t>(reg);
    // x0 不允许重命名
    return index > 0U && index < 32U;
}

void RenameTable::latch(){
    cur_table_ = next_table_;
    cur_table_[0] = RobTag{};
}

bool RenameTable::busy(RegisterIndex reg){
    return lookup(reg).valid;
}

void RenameTable::evaluate_updates(const RATWrite& issue_write, const RATCommit& commit, bool flush){
    next_table_ = cur_table_;
    // 当前设计在控制指令到达 ROB 队首时进行错误预测恢复。
    // 此时 ROB 中剩余指令全部比该控制指令年轻，因此可以清空 RAT。
    if(flush){
        invalidate(next_table_);
        return;
    }
    // 先处理老指令的 Commit
    // 只有 RAT 仍指向正在提交的 tag 时才允许清除，防止老指令
    // 错误清除同一寄存器上更年轻指令的映射
    if(commit.valid && commit.tag.valid && valid_register(commit.rd)){
        const std::size_t index = static_cast<std::size_t>(commit.rd);
        if(next_table_[index] == commit.tag){
            next_table_[index] = RobTag{};
        }
    }
    // 再处理年轻指令的 Issue。
    // 当 Commit 和 Issue 在同周期操作同一个 rd 时，新发射指令的 tag 最终保留在 RAT 中。
    if(issue_write.valid && issue_write.tag.valid && valid_register(issue_write.rd)){
        const std::size_t index = static_cast<std::size_t>(issue_write.rd);
        next_table_[index] == issue_write.tag;
    }
    next_table_[0] = RobTag{};
}