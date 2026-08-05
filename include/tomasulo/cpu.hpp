#pragma once

#include <istream>
#include "common/types.hpp"
#include "memory/memory_unit.hpp"
#include "tomasulo/branch_predictor.hpp"
#include "tomasulo/common_data_bus.hpp"
#include "tomasulo/functional_unit.hpp"
#include "tomasulo/issue_unit.hpp"
#include "tomasulo/load_store_queue.hpp"
#include "tomasulo/register_file.hpp"
#include "tomasulo/rename_table.hpp"
#include "tomasulo/reorder_buffer.hpp"
#include "tomasulo/reservation_station.hpp"


class CPU{
public:
    enum class Status{
        NotLoaded,
        Running,
        Halted,
        LoadError,
        FetchOutOfBound,
        InvalidInstruction,
        MemoryOutOfBound,
        InternalError
    };

    struct RunResult{
        Status status = Status::NotLoaded;
        Word value = 0;

        bool halted() const  {
            return status == Status::Halted;
        }

        bool failed() const  {
            return status != Status::Running && status != Status::Halted;
        }
    };

    CPU() = default;

    ImageLoadResult load(std::istream& input);

    void cycle();

    RunResult run();

    Address pc() const;

    Status status() const;

    CycleCount cycle_count() const;
    CycleCount committed_count() const;

    CycleCount branch_count() const;
    CycleCount correct_branch_count() const;
    double branch_accuracy() const;

    Word register_value(RegisterIndex index) const;

private:
    struct ROBPreview{
        StoreCommitRequest store_request{};

        bool flush = false;
        Address redirect_pc = 0;
    };
    MemoryUnit memory_unit_{};
    RegisterFile register_file_{};
    RenameTable rename_table_{};
    ReorderBuffer rob_{};
    ReservationStation reservation_station_{};
    LoadStoreQueue load_store_queue_{};
    FunctionalUnit functional_unit_{};
    CommonDataBus common_data_bus_{};
    IssueUnit issue_unit_{};
    BranchPredictor branch_predictor_{};

    Address current_pc_ = 0;
    Address next_pc_ = 0;

    // HALT 已经进入 ROB 后停止继续取年轻指令。
    // 如果 HALT 在错误路径上，Flush 会将该状态清除。
    bool fetch_stopped_ = false;
    bool next_fetch_stopped_ = false;

    Status status_ = Status::NotLoaded;

    CycleCount cycle_count_ = 0;
    CycleCount committed_count_ = 0;

    Word exit_code_ = 0;

    SourceState source_state(RegisterIndex reg);

    Address predicted_next_pc(const DecodedInstruction& instruction, Address pc, bool& predicted_taken) const;

    ROBPreview preview_rob() const;

    DecodePacket fetchAndDecoded(bool &fetch_error);

    void latch_all();
};