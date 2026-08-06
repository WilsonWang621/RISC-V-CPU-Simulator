#include "cpu.hpp"
#include "common/config.hpp"
#include "isa/decoder.hpp"

#include <algorithm>
#include <array>
#include <cassert>

namespace {

enum class PlanModule {
    Issue,
    ReorderBuffer,
    ReservationStation,
    LoadStoreQueue
};

enum class ApplyModule {
    CommonDataBus,
    ReorderBuffer,
    ReservationStation,
    LoadStoreQueue,
    FunctionalUnit,
    RegisterFile,
    RenameTable,
    BranchPredictor,
    MemoryUnit
};

enum class LatchModule {
    RegisterFile,
    RenameTable,
    ReorderBuffer,
    ReservationStation,
    LoadStoreQueue,
    FunctionalUnit,
    CommonDataBus,
    MemoryUnit,
    BranchPredictor
};

}

CPU::CPU(std::uint32_t random_seed)
    : random_engine_(random_seed) {
    initialize_execution_orders();
}

void CPU::initialize_execution_orders() {
    constexpr PlanOrder base_plan{{0U, 1U, 2U, 3U}};
    constexpr ApplyOrder base_apply{{0U, 1U, 2U, 3U, 4U, 5U, 6U, 7U, 8U}};
    constexpr LatchOrder base_latch{{0U, 1U, 2U, 3U, 4U, 5U, 6U, 7U, 8U}};

    for (std::size_t index = 0U; index < kOrderVariantCount; ++index) {
        plan_orders_[index] = base_plan;
        apply_orders_[index] = base_apply;
        latch_orders_[index] = base_latch;

        std::shuffle(plan_orders_[index].begin(), plan_orders_[index].end(), random_engine_);
        std::shuffle(apply_orders_[index].begin(), apply_orders_[index].end(), random_engine_);
        std::shuffle(latch_orders_[index].begin(), latch_orders_[index].end(), random_engine_);
    }
}

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

DecodePacket CPU::fetchAndDecoded(bool& fetch_error) const{
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

SourceState CPU::source_state(RegisterIndex reg) const{
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

    // Phase A: 只观察 current state，冻结本周期的组合 wire。
    const ROBPreview rob_preview = preview_rob();

    CDBInputs cdb_inputs{};
    if(!rob_preview.flush){
        cdb_inputs.integer_result = functional_unit_.result();
        cdb_inputs.load_result = load_store_queue_.load_result();
    }

    const CDBDecision cdb_decision = common_data_bus_.plan(cdb_inputs);
    const CDBOutputs& cdb_outputs = cdb_decision.outputs;

    LSQObserveInputs lsq_observe{};
    lsq_observe.store_request = rob_preview.store_request;
    lsq_observe.memory_response = memory_unit_.data_response();
    lsq_observe.memory_available = memory_unit_.data_port_available();
    lsq_observe.load_result_granted = cdb_outputs.load_granted;
    lsq_observe.flush = rob_preview.flush;

    // Phase B: plan() 只读 current state/CycleView。四个模块的输出
    // 互不依赖调用顺序，可以每周期随机打乱。
    IssueOutputs issue_outputs{};
    ROBDecision rob_decision{};
    RSDecision rs_decision{};
    LSQDecision lsq_decision{};
    bool fetch_error = false;

    const std::size_t order_index = static_cast<std::size_t>(cycle_count_ % kOrderVariantCount);
    const PlanOrder& plan_order = plan_orders_[order_index];

    for (const std::uint8_t raw_module : plan_order) {
        const PlanModule module = static_cast<PlanModule>(raw_module);
        switch (module) {
            case PlanModule::Issue: {
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

                        issue_outputs = issue_unit_.plan(issue_inputs);
                    }
                }
                break;
            }
            case PlanModule::ReorderBuffer:
                rob_decision = rob_.plan();
                break;
            case PlanModule::ReservationStation:
                rs_decision = reservation_station_.plan(rob_preview.flush);
                break;
            case PlanModule::LoadStoreQueue:
                lsq_decision = load_store_queue_.plan(lsq_observe);
                break;
        }
    }

    // 从这里开始所有模块输出已经冻结。
    ROBOutputs rob_outputs = rob_decision.outputs;
    RSOutputs rs_outputs = rs_decision.outputs;
    LSQOutputs lsq_outputs = lsq_decision.outputs;

    const bool issue_effective = issue_outputs.issued() && !rob_outputs.flush;

    LSQInputs lsq_inputs{};

    lsq_inputs.issue_valid = issue_outputs.write_lsq && !rob_preview.flush;
    lsq_inputs.issue_entry = issue_outputs.lsq_entry;
    lsq_inputs.cdb = cdb_outputs.broadcast;
    lsq_inputs.flush = rob_preview.flush;
    lsq_inputs.load_result_granted = cdb_outputs.load_granted;
    lsq_inputs.memory_available = memory_unit_.data_port_available();
    lsq_inputs.memory_response = memory_unit_.data_response();
    lsq_inputs.store_request = rob_preview.store_request;

    ROBInputs rob_inputs{};

    rob_inputs.issue_valid = issue_outputs.write_rob && !rob_preview.flush;
    rob_inputs.issue_entry = issue_outputs.rob_entry;
    rob_inputs.completion = cdb_outputs.completion;
    rob_inputs.store_completion = lsq_outputs.store_completion;

    if (issue_effective) {
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

    RSInputs rs_inputs{};

    rs_inputs.cdb = cdb_outputs.broadcast;
    rs_inputs.flush = rob_preview.flush;
    rs_inputs.fu_available = functional_unit_.can_accept(cdb_outputs.integer_granted);
    rs_inputs.issue_entry = issue_outputs.rs_entry;
    rs_inputs.issue_valid = issue_outputs.write_rs && issue_effective;

    const Execute execute = rs_outputs.dispatch;
    RegisterWrite register_write{};
    
    if(rob_outputs.commit.valid && rob_outputs.commit.entry.writes_rd && rob_outputs.commit.valid){
        register_write.rd = rob_outputs.commit.entry.rd;
        register_write.value = rob_outputs.commit.entry.value;
        register_write.valid = true;
    }

    RATWrite rat_write{};

    if(issue_outputs.rename.valid && issue_effective){
        rat_write.rd = issue_outputs.rename.rd;
        rat_write.tag = issue_outputs.rename.tag;
        rat_write.valid = true;
    }

    RATCommit rat_commit{};

    if(rob_outputs.commit.valid && rob_outputs.commit.entry.writes_rd){
        rat_commit.rd = rob_outputs.commit.entry.rd;
        rat_commit.tag = rob_outputs.commit.entry.tag;
        rat_commit.valid = true;
    }

    BranchPredictorUpdate predictor_update{};

    if (rob_outputs.commit.valid &&
        rob_outputs.commit.entry.is_branch) {
        const ROBEntry& entry = rob_outputs.commit.entry;

        predictor_update.pc = entry.pc;
        predictor_update.actual_taken = entry.branch_taken;
        predictor_update.predicted_next_pc = entry.predicted_next_pc;
        predictor_update.actual_next_pc = entry.actual_next_pc;
        predictor_update.valid = true;
    }

    // Phase C: 所有 apply 输入已经冻结。每个调用只写自己的 next
    // state，因此执行顺序可以随机交换。
    bool rob_issue_accepted = false;
    bool rs_issue_accepted = false;
    bool lsq_issue_accepted = false;
    bool fu_accepted = false;
    bool memory_accepted = false;

    const ApplyOrder& apply_order = apply_orders_[order_index];

    for (const std::uint8_t raw_module : apply_order) {
        const ApplyModule module = static_cast<ApplyModule>(raw_module);
        switch (module) {
            case ApplyModule::CommonDataBus:
                common_data_bus_.apply(cdb_decision);
                break;
            case ApplyModule::ReorderBuffer:
                rob_issue_accepted = rob_.apply(rob_inputs, rob_decision);
                break;
            case ApplyModule::ReservationStation:
                rs_issue_accepted = reservation_station_.apply(rs_inputs, rs_decision);
                break;
            case ApplyModule::LoadStoreQueue:
                lsq_issue_accepted = load_store_queue_.apply(lsq_inputs, lsq_decision);
                break;
            case ApplyModule::FunctionalUnit:
                fu_accepted = functional_unit_.apply(execute, rob_preview.flush, cdb_outputs.integer_granted);
                break;
            case ApplyModule::RegisterFile:
                register_file_.apply(register_write);
                break;
            case ApplyModule::RenameTable:
                rename_table_.apply(rat_write, rat_commit, rob_outputs.flush);
                break;
            case ApplyModule::BranchPredictor:
                branch_predictor_.apply(predictor_update);
                break;
            case ApplyModule::MemoryUnit:
                memory_accepted = memory_unit_.apply(lsq_outputs.memory_request);
                break;
        }
    }

    if (issue_effective) {
        assert(rob_issue_accepted);
        if (issue_outputs.write_rs) {
            assert(rs_issue_accepted);
        }
        if (issue_outputs.write_lsq) {
            assert(lsq_issue_accepted);
        }
    }

    if (rs_outputs.dispatch_valid && rs_inputs.fu_available && !rob_preview.flush) {
        assert(fu_accepted);
    }

    if(lsq_outputs.memory_request.valid){
        assert(memory_accepted);
    }

    Status next_status = status_;

    if (rob_outputs.commit.valid && rob_outputs.commit.entry.op != OP::HALT) {
        ++committed_count_;
    }

    // 取指或译码错误可能来自尚未确认的推测路径。ROB 非空时
    // 保持当前 PC，让更老的指令先完成；若随后发生 Flush，错误
    // 路径会被重定向。只有 ROB 已空时才能确认错误属于正确路径。
    if (fetch_error && rob_.empty()) {
        next_status = Status::FetchOutOfBound;
    }

    if (issue_outputs.status == IssueStatus::InvalidInstruction && rob_.empty()) {
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

    // Phase D: 所有 next state 已经完成，latch 顺序同样随机。
    latch_all();

    current_pc_ = next_pc_;
    fetch_stopped_ = next_fetch_stopped_;
    status_ = next_status;
}

CPU::RunResult CPU::run() {
    if (status_ != Status::Running) {
        return RunResult{status_, exit_code_};
    }

    while (status_ == Status::Running) {
        cycle();
    }

    return RunResult{status_, exit_code_};
}

void CPU::latch_all() {
    const std::size_t order_index = static_cast<std::size_t>(cycle_count_ % kOrderVariantCount);
    const LatchOrder& latch_order = latch_orders_[order_index];

    for (const std::uint8_t raw_module : latch_order) {
        const LatchModule module = static_cast<LatchModule>(raw_module);
        switch (module) {
            case LatchModule::RegisterFile:
                register_file_.latch();
                break;
            case LatchModule::RenameTable:
                rename_table_.latch();
                break;
            case LatchModule::ReorderBuffer:
                rob_.latch();
                break;
            case LatchModule::ReservationStation:
                reservation_station_.latch();
                break;
            case LatchModule::LoadStoreQueue:
                load_store_queue_.latch();
                break;
            case LatchModule::FunctionalUnit:
                functional_unit_.latch();
                break;
            case LatchModule::CommonDataBus:
                common_data_bus_.latch();
                break;
            case LatchModule::MemoryUnit:
                memory_unit_.latch();
                break;
            case LatchModule::BranchPredictor:
                branch_predictor_.latch();
                break;
        }
    }
}
