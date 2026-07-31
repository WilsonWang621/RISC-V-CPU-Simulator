#pragma once

#include<array>
#include<cstddef>

#include"types.hpp"

static constexpr std::size_t kRegisterCount = 32U; 

struct RATWrite{
    RegisterIndex rd = 0;
    RobTag tag{};
    bool valid = false;
};

struct RATCommit{
    RegisterIndex rd = 0;
    RobTag tag{};
    bool valid = false;
};

class RenameTable{  //功能：记录每个架构寄存器最新的、尚未提交的生产者 ROB tag
public:
    RenameTable();

    // 查询当前周期旧状态中的寄存器生产者。
    // 返回 invalid tag 表示该寄存器没有未提交生产者。
    RobTag lookup(RegisterIndex idx);

    // 装载新程序时清空所有重命名关系
    void reset();

    void latch();

    bool busy(RegisterIndex reg);

    // 根据本周期 Issue、Commit 和 Flush 信号计算下一状态
    // 优先级：flush > issue write > commit clear
    // 因此同周期写同一 rd 时，年轻的 Issue 映射最终保留下来
    void evaluate_updates(const RATWrite& issue_write, const RATCommit& commit, bool flush);
private:
    std::array<RobTag, kRegisterCount> cur_table_{};
    std::array<RobTag, kRegisterCount> next_table_{};

    void RenameTable::invalidate(std::array<RobTag, kRegisterCount>& table);

    bool valid_register(RegisterIndex idx);
};


