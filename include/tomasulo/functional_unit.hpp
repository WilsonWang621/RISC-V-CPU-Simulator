#pragma once

#include"signals.hpp"

class FunctionalUnit{
public:
    FunctionalUnit();

    void reset();
    void latch();

    // 当前是否有一个执行完成、等待广播的结果。
    bool has_result() const;
    
    //读取当前cycle的结果 但是不修改状态
    FUResult result() const;

    // 如果当前结果本周期会被 CDB 接收，那么 FU 可以同时接收
    // 下一条指令，从而达到每周期一条的吞吐量。
    bool can_accept(bool result_accepted) const;

    // 计算 FU 下一状态
    // execute:RS 本周期尝试派发的指令。
    // result_accepted:当前 current_result_ 是否获得 CDB 仲裁。
    // flush: 分支恢复信号，优先级最高。
    // 返回值表示 execute 是否被 FU 接收
    bool apply(const Execute& execute, bool flush, bool result_accepted);

private:
    FUResult current_result_{};
    FUResult next_result_{};

    static FUResult execute_instruction(const Execute& execute);

    static bool signed_greater_equal(Word lhs, Word rhs);
    static bool signed_less_than(Word lhs, Word rhs);

    static Word arithmetic_shift_right(Word value, unsigned amount);
};
