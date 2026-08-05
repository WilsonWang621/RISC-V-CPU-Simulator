#include "cpu.hpp"
#include "common/config.hpp"
#include "isa/decoder.hpp"

#include <cassert>

ImageLoadResult CPU::load(std::istream &input){
    //所有成员变量初始化
    register_file_.reset();
    rename_table_.reset();
    rob_.reset();
    reservation_station_.reset();
    load_store_queue_.reset();
    functional_unit_.reset();
    common_data_bus_.reset();
    branch_predictor_.reset();
    
    memory_unit_.reset();

    current_pc_ = 0;
    next_pc_ = 0;

    fetch_stopped_ = false;
    next_fetch_stopped_ = false;

    cycle_count_ = 0;
    committed_count_ = 0;
    exit_code_ = 0;

    const ImageLoadResult result = memory_unit_.load(input);

    status_ = result.ok() ? Status::Running : Status::LoadError;
    return result;
}

Address CPU::pc() const{
    return current_pc_;
}

CPU::Status CPU::status() const{
    return status_;
}

CycleCount CPU::cycle_count() const{
    return cycle_count_;
}

CycleCount CPU::committed_count() const{
    return committed_count_;
}

CycleCount CPU::branch_count() const{
    return branch_predictor_.prediction_count();
}

CycleCount CPU::correct_branch_count() const{
    return branch_predictor_.correct_count();
}

double CPU::branch_accuracy() const{
    return branch_predictor_.accuracy();
}

Word CPU::register_value(RegisterIndex index) const{
    return register_file_.read(index);
}

CPU::ROBPreview CPU::preview_rob() const{
    ROBPreview preview{};
    const ROBEntry* head = rob_.front();
    if(head == nullptr){
        return preview;
    }

    if(head->is_store && !head->ready){
        preview.store_request.tag = head->tag;
        preview.store_request.valid = true;
    }

    const bool is_control = head->is_branch || head->is_jump;
    if(is_control && head->ready){
        preview.flush = head->predicted_next_pc != head->actual_next_pc;
        if(preview.flush){
            preview.redirect_pc = head->actual_next_pc;
        }
    }
    return preview;
}

Address CPU::predicted_next_pc(const DecodedInstruction& instruction, Address pc, bool& predicted_taken) const{
    const Address sequential = pc + static_cast<Address>(kInstructionWidth);

    predicted_taken = false;
    if(instruction.is_branch){
        predicted_taken = branch_predictor_.predict_taken(pc);
        return predicted_taken ? pc + static_cast<Address>(instruction.immediate) : sequential;
    }

    if (instruction.op == OP::JAL) {
        predicted_taken = true;
        return pc + static_cast<Address>(instruction.immediate);
    }

    // JALR 目标依赖 rs1，基础版本预测顺序执行
    if (instruction.op == OP::JALR) {
        return sequential;
    }

    return sequential;
}

DecodePacket CPU::fetchAndDecoded(bool& fetch_error){
    fetch_error = false;

    InstructionFetchRequest request{};
    request.pc = current_pc_;
    request.valid = true;

    const InstructionFetchResponse response = memory_unit_.fetch(request);
    if(!response.valid || !response.success){
        fetch_error = true;
        return DecodePacket{};
    }

    DecodePacket packet{};
    packet.instruction = response.instruction;
    packet.pc = response.pc;
    packet.decoded = Decoder::decode(response.instruction);

    packet.predicted_pc = predicted_next_pc(packet.decoded, packet.pc, packet.predicted_taken);

    packet.valid = true;
    return packet;
}

SourceState CPU::source_state(RegisterIndex reg){
    SourceState source{};
    
    if (reg == RegisterIndex{0}) {
        source.architectural_value = 0;
        return source;
    }

    source.architectural_value = register_file_.read(reg);
    source.producer = rename_table_.lookup(reg);

    if (!source.producer.valid) {
        return source;
    }
    
    const ROBEntry* producer = rob_.lookup(source.producer);
    if(producer != nullptr && producer->ready){
        source.producer_ready = true;
        source.producer_value = producer->value;
    }
    return source;
}

void CPU::cycle(){
    if (status_ != Status::Running) {
        return;
    }

    ++cycle_count_;

    next_pc_ = current_pc_;
    next_fetch_stopped_ = fetch_stopped_;
    /*
    * 1. 预览 ROB 当前队首
    * Flush 和 Store 授权都只依赖 ROB 当前状态。
    */
    const ROBPreview rob_preview = preview_rob();

    /*
    * 2. CDB 仲裁当前完成结果。
    * FU 和 LSQ 的 result() 都读取当前状态。
    */
    CDBInputs cdb_inputs;
    if(!rob_preview.flush){
        cdb_inputs.integer_result = functional_unit_.result();
        cdb_inputs.load_result = load_store_queue_.load_result();
    }

    const CDBOutputs cdb_outputs = common_data_bus_.evaluate(cdb_inputs);

    /*
    * 3. Fetch、Decode、Issue。
    * ROB 满时所有指令都无法发射，因此直接 Stall，
    * 同时避免当前 IssueUnit 把无效 next_tag 误判成错误。
    */
    IssueOutputs issue_outputs{};
    bool fetch_error = false;

    if(!rob_preview.flush && !fetch_stopped_ && !rob_.full()){
       const DecodePacket packet = fetchAndDecoded(fetch_error);
       if(!fetch_error){
          IssueInputs issue_inputs{};
          issue_inputs.packet = packet;
          issue_inputs.allocated_tag = rob_.next_tag();
          issue_inputs.cdb = cdb_outputs.broadcast;
          issue_inputs.lsq_available = !load_store_queue_.full();
          issue_inputs.rob_available = !rob_.full();
          issue_inputs.rs1 = source_state(packet.decoded.rs1);
          issue_inputs.rs2 = source_state(packet.decoded.rs2);
          issue_inputs.rs_available = reservation_station_.available();

          issue_outputs = issue_unit_.evaluate(issue_inputs);
       }
    }

    /*
    * 4. LSQ evaluate
    * 它需要：
    *   - 当前 Memory Response；
    *   - 当前 CDB；
    *   - ROB 当前 Store 授权；
    *   - CDB 是否接收 Load Result。
    */

    LSQInputs lsq_inputs;

    lsq_inputs.issue_valid = issue_outputs.write_lsq && !rob_preview.flush;
    lsq_inputs.issue_entry = issue_outputs.lsq_entry;
    lsq_inputs.cdb = cdb_outputs.broadcast;
    lsq_inputs.flush = rob_preview.flush;
    lsq_inputs.load_result_granted = cdb_outputs.load_granted;
    lsq_inputs.memory_available = memory_unit_.data_port_available();
    lsq_inputs.memory_response = memory_unit_.data_response();
    lsq_inputs.store_request = rob_preview.store_request;

    const LSQOutputs lsq_outputs = load_store_queue_.evaluate(lsq_inputs);

    /*
     * 5. ROB evaluate。
     * LSQ 本周期产生的 Store Completion 可以进入 ROB next 状态。
     */
    ROBInputs rob_inputs;

    rob_inputs.issue_valid = issue_outputs.write_rob && !rob_preview.flush;
    rob_inputs.issue_entry = issue_outputs.rob_entry;
    rob_inputs.completion = cdb_outputs.completion;
    rob_inputs.store_completion = lsq_outputs.store_completion;

    const ROBOutputs rob_outputs = rob_.evaluate(rob_inputs);

    /*
     * 6. 确定本周期 Issue 是否真正生效。
     */
    const bool issue_effective = issue_outputs.issued() && !rob_outputs.flush;

    if (issue_effective) {
        assert(rob_outputs.issue_accepted);

        if (issue_outputs.write_lsq) {
            assert(lsq_outputs.issue_accepted);
        }

        next_pc_ = issue_outputs.rob_entry.predicted_next_pc;

        if (issue_outputs.rob_entry.op == OP::HALT) {
            // HALT 已进入 ROB，停止继续取年轻指令。
            next_fetch_stopped_ = true;
        }
    }

    if (rob_outputs.flush) {
        next_pc_ = rob_outputs.redirect_pc;

        // 错误路径上的 HALT 也必须撤销。
        next_fetch_stopped_ = false;
    }

    /*
     * 7. RS evaluate。
     *
     * FU 当前结果若获得 CDB 仲裁，本周期可以同时接收新指令。
     */
    RSInputs rs_inputs;

    rs_inputs.cdb = cdb_outputs.broadcast;
    rs_inputs.flush = rob_preview.flush;
    rs_inputs.fu_available = functional_unit_.can_accept(cdb_outputs.integer_granted);
    rs_inputs.issue_entry = issue_outputs.rs_entry;
    rs_inputs.issue_valid = issue_outputs.write_rs && issue_effective;

    const RSOutputs rs_outputs = reservation_station_.evaluate(rs_inputs);

    /*
    * 8. FU evaluate。
    */
    
    Execute execute = rs_outputs.dispatch;
    const bool fu_accepted = functional_unit_.evaluate(execute, rob_preview.flush, cdb_outputs.integer_granted);


    if(rs_outputs.dispatch_valid && rs_outputs.dispatch_valid && !rob_preview.flush){
        assert(fu_accepted);
    }

    /*
     * 9. Register File Commit。
     */
    RegisterWrite register_write;
    
    if(rob_outputs.commit.valid && rob_outputs.commit.entry.writes_rd && rob_outputs.commit.valid){
        register_write.rd = rob_outputs.commit.entry.rd;
        register_write.value = rob_outputs.commit.entry.value;
        register_write.valid = true;
    }

    register_file_.evaluate_commit(register_write);

    /*
     * 10. RAT：Commit 清除 + Issue 写入 + Flush。
     */
    RATWrite rat_write;

    if(issue_outputs.rename.valid && issue_effective){
        rat_write.rd = issue_outputs.rename.rd;
        rat_write.tag = issue_outputs.rename.tag;
        rat_write.valid = true;
    }

    RATCommit rat_commit;

    if(rob_outputs.commit.valid && rob_outputs.commit.entry.writes_rd){
        rat_commit.rd = rob_outputs.commit.entry.rd;
        rat_commit.tag = rob_outputs.commit.entry.tag;
        rat_commit.valid = true;
    }

    rename_table_.evaluate_updates(rat_write, rat_commit, rob_outputs.flush);

    /*
     * 11. Branch Predictor Commit 更新。
     */
    BranchPredictorUpdate predictor_update;

    if (rob_outputs.commit.valid &&
        rob_outputs.commit.entry.is_branch) {
        const ROBEntry& entry = rob_outputs.commit.entry;

        predictor_update.pc = entry.pc;
        predictor_update.actual_taken = entry.branch_taken;
        predictor_update.predicted_next_pc = entry.predicted_next_pc;
        predictor_update.actual_next_pc = entry.actual_next_pc;
        predictor_update.valid = true;
    }

    branch_predictor_.evaluate(predictor_update);

    /*
    * 12. Memory Unit 推进数据事务。
    */

    const bool memory_accepted = memory_unit_.evaluate(lsq_outputs.memory_request);
    if(lsq_outputs.memory_request.valid){
        assert(memory_accepted);
    }

    /*
     * 13. 统计和状态。
     */
    Status next_status = status_;

    if (rob_outputs.commit.valid && rob_outputs.commit.entry.op != OP::HALT) {
        ++committed_count_;
    }

    if (fetch_error) {
        next_status = Status::FetchOutOfBound;
    }

    if (issue_outputs.status == IssueStatus::InvalidInstruction) {
        next_status = Status::InvalidInstruction;
    }

    if (issue_outputs.status == IssueStatus::InvalidAllocation) {
        next_status = Status::InternalError;
    }

    if (lsq_outputs.memory_error) {
        next_status = Status::MemoryOutOfBound;
    }

    /*
     * HALT 只在 ROB 队首提交时生效。
     *
     * 由于基础设计每周期只提交一条，HALT 前面的指令已经在
     * 更早周期写入 Register File。
     */
    if (rob_outputs.commit.halted) {
        exit_code_ = register_file_.read(10) & Word{0xffU};
        next_status = Status::Halted;
    }

    /*
     * 14. 周期末统一锁存。
     */
    latch_all();

    current_pc_ = next_pc_;
    fetch_stopped_ = next_fetch_stopped_;
    status_ = next_status;
}

CPU::RunResult CPU::run() {
    if (status_ != Status::Running) {
        return RunResult{
            status_,
            exit_code_
        };
    }

    while (status_ == Status::Running) {
        cycle();
    }

    return RunResult{
        status_,
        exit_code_
    };
}

void CPU::latch_all() {
    register_file_.latch();
    rename_table_.latch();

    rob_.latch();
    reservation_station_.latch();
    load_store_queue_.latch();

    functional_unit_.latch();
    common_data_bus_.latch();

    memory_unit_.latch();
    branch_predictor_.latch();
}
