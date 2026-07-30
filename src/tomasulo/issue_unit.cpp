#include "tomasulo/issue_unit.hpp"


namespace{
    bool is_memory_instruction(const DecodedInstruction& ins){
        return ins.is_load || ins.is_store;
    }
}

Operand IssueUnit::ready_operand(Word v){
    Operand result{};
    result.value = v;
    result.ready = true;
    return result;
}

Operand IssueUnit::resolve_register(RegisterIndex idx, const SourceState& src, const CDBMsg &msg){
    Operand result{};
    if(idx == RegisterIndex{0}){
        return ready_operand(Word{0});
    }

    if(!src.producer.valid){ //当前register没有未提交producer
        return ready_operand(src.architectural_value);
    }

    // 同周期 CDB 广播优先旁路。
    // ps: 每个cycle里面broadcast早于issue
    // 这可以处理“生产者本周期广播，消费者本周期发射”的情况，
    // 而不需要让消费者额外等待一个周期。
    if (msg.valid && msg.tag.valid && msg.tag == src.producer) {
        return ready_operand(msg.value);
    }

    // 生产者已经在 ROB 中完成，直接从 ROB 旁路结果。
    if(src.producer_ready){
        return ready_operand(src.producer_value);
    }

    result.ready = false;
    result.tag = src.producer;
    return result;
}

Operand IssueUnit::select_operand(OperandSource src, const Operand& register_operand, Word immediate, Address pc){
    switch (src){
        case OperandSource::Immediate: return ready_operand(immediate);
        case OperandSource::ProgramCounter: return ready_operand(static_cast<Word>(pc));
        case OperandSource::Register : return register_operand;
        case OperandSource::Zero : return ready_operand(Word{0});
    }
    return ready_operand(Word{0});
}

IssueOutputs IssueUnit::evaluate(const IssueInputs &inputs){
    IssueOutputs outputs{};
    //先检验输入有效性
    if(inputs.packet.valid ==  false){
        return outputs;
    }

    if(inputs.packet.decoded.op == OP::INVALID){
        outputs.status = IssueStatus::InvalidInstruction;
        return outputs;
    }

    //分配tag是否有效
    if(!inputs.allocated_tag.valid){
        outputs.status = InvalidAllocation;
        return outputs;
    }
    const DecodedInstruction& instruction = inputs.packet.decoded;

    //检验目标的availability
    bool need_lsq = is_memory_instruction(instruction);
    const bool is_halt = instruction.op == OP::HALT;
    bool need_rs = !is_halt && !need_lsq;

    if(!inputs.rob_available || (need_lsq && !inputs.lsq_available) || (need_rs && !inputs.rs_available)){
        outputs.status = Unavailability;
        return outputs;
    }

    /*正式开始处理： 
    注意在这里只是提供每个结构处理需要的信息
    真正的排队等待实在各自对应文件中实现的，这里只是发布
    */

    //先准备rs1 rs2
    const Operand rs1 = resolve_register(instruction.rs1, inputs.rs1, inputs.cdb);
    const Operand rs2 = resolve_register(instruction.rs2, inputs.rs2, inputs.cdb);

    //（1）ROB 这是所有指令都需要的
    ROBEntry rob_entry{};
    rob_entry.tag = inputs.allocated_tag;
    rob_entry.op = instruction.op;
    rob_entry.pc = inputs.packet.pc;
    rob_entry.rd = instruction.rd;
    rob_entry.writes_rd =  instruction.writes_rd && instruction.rd != RegisterIndex{0};
    rob_entry.ready = is_halt;      //HALT指令不执行
    rob_entry.predicted_next_pc = inputs.packet.predicted_pc;
    rob_entry.is_branch = instruction.is_branch;
    rob_entry.is_store = instruction.is_store;
    rob_entry.valid = true;

    outputs.rob_entry = rob_entry;
    outputs.write_rob = true;

    
    //IF 需要rs
    if(need_rs){
        RSEntry entry{};
        entry.op = instruction.op;
        entry.destination = inputs.allocated_tag;
        entry.lhs = select_operand(instruction.lhs_source, rs1, instruction.immediate, inputs.packet.pc);
        entry.rhs = select_operand(instruction.rhs_source, rs2, instruction.immediate, inputs.packet.pc);
        entry.immediate = instruction.immediate;
        entry.pc = inputs.packet.pc;
        entry.predicted_next_pc = inputs.packet.predicted_pc;
        entry.busy = true;

        outputs.write_rs = true;
        outputs.rs_entry = entry;
    }

    if(need_lsq){
        LSQEntry entry{};
        entry.type = instruction.is_load ? LSType::LOAD : LSType::STORE;
        entry.op = instruction.op;
        entry.destination = inputs.allocated_tag;
        entry.base = rs1;
        entry.store_data = instruction.is_store ? rs2 : ready_operand(Word{0});
        entry.immediate = instruction.immediate;
        entry.address = 0;
        entry.address_ready = false;
        entry.request_sent = false;
        entry.valid = true;

        outputs.lsq_entry = entry;
        outputs.write_lsq = true;
    }
    //Store 读 RAT，但不更新 RAT
    if(rob_entry.writes_rd){ //store 对应的instruction.writes_rd == false 
        outputs.rename.rd = instruction.rd;
        outputs.rename.tag = inputs.allocated_tag;
        outputs.rename.valid = true;
    }

    outputs.pop_decode = true;
    outputs.status = IssueStatus::Issued;
    return outputs;
}