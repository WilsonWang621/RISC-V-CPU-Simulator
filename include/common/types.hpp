#pragma once
#include<stdint.h>

enum OP{
    ADD,
    SUB,
    AND,
    XOR,
    OR,
};

//memory -> instruction fetch
struct FetchMsg{
    uint64_t pc;         //指令地址
    uint32_t instruction;//机器码

    uint64_t predicted_pc;//预测下一pc
    bool predicted_taken; //分支预测结果
};

struct DecodeMsg{
    uint64_t pc;

};