#pragma once
#include"signals.hpp"
#include"load_store_queue.hpp"
#include"reservation_station.hpp"
#include"reorder_buffer.hpp"

enum IssueStatus{
    InvalidInstruction, //指令无效
    InvalidAllocation,  //错误分配
    Unavailability,     //目标分配结构没有空闲
    Issued,   //成功发射
    Empty     //初始为空
};

struct RenameUpdate{
    RegisterIndex rd = 0;
    RobTag tag{};
    bool valid = false;
};

// producer.valid == false:
//     当前寄存器没有未提交生产者，使用 architectural_value。
//
// producer.valid == true:
//     当前寄存器由 producer 指向的 ROB 项产生。
//     若 producer_ready 为 true，可以直接旁路 producer_value；
//     否则生成一个等待该 tag 的 Operand。
struct SourceState{
    Word architectural_value = 0;
    RobTag producer{};
    Word producer_value = 0; 
    bool producer_ready = false;
};

struct IssueInputs{ 
    //是否发布要看这条指令是什么类型，对应需求的ROB RS LSQ有没有位置
    DecodePacket packet{}; //指令信息

    //ROB 根据旧状态预留的下一个 ta
    RobTag allocated_tag{};

    // packet.decoded.rs1 / rs2 对应的读取快照
    SourceState rs1{};
    SourceState rs2{};

    // 同周期 CDB 广播旁路。
    CDBMsg cdb{};

    //目标需求的状态
    bool rob_available = false;
    bool rs_available = false;
    bool lsq_available = false;
};

struct IssueOutputs{
    IssueStatus status = IssueStatus::Empty;
    //发布信息
    ROBEntry rob_entry{};
    RSEntry rs_entry{};
    LSQEntry lsq_entry{};
    RenameUpdate rename{};

    bool pop_decode = false; //issue unit 已经接收发布，上游可以从decode queue中删除它
    bool write_rob = false;
    bool write_rs = false;
    bool write_lsq = false;

    bool issued() const{
        return status == IssueStatus::Issued;
    }
};


class IssueUnit{ 
public:
    IssueOutputs plan(const IssueInputs& inputs) const;

private:
    static Operand ready_operand(Word value);

    //register状态来源下对应的结果
    static Operand resolve_register(RegisterIndex idx, const SourceState& src, const CDBMsg &msg);

    //4种状态来源里面选择一种
    static Operand select_operand(OperandSource src, const Operand& register_operand, Word immediate, Address pc);
};
