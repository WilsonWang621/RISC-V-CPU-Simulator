#pragma once

#include<array>
#include<cstddef>

#include"common/types.hpp"

struct RegisterWrite{ 
    RegisterIndex rd = 0;
    Word value = 0;
    bool valid = false;
};


class RegisterFile{
public:
    static constexpr std::size_t kRegisterCount = 32U;

    RegisterFile();

    void reset();

    // 读取当前周期的已提交寄存器状态。
    // x0 和非法寄存器编号都返回 0。
    Word read(RegisterIndex idx) const;

    const std::array<Word, kRegisterCount>& values() const;

    // 记录本周期 ROB Commit 提供的写请求。
    // 该函数不会立即改变 read() 返回的值。
    void apply(const RegisterWrite& write);

    // 周期结束时锁存写入，并清除本周期写请求
    void latch();

private:
    std::array<Word, kRegisterCount> register_;
    RegisterWrite pending_write_{};  //这个周期要待入的信息
};
