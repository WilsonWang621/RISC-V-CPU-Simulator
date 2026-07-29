#pragma once

#include"common/types.hpp"
#include"memory/image.hpp"
#include<array>
#include<istream>

class Interpreter{
public:
    enum class Status{
        kNotLoaded,
        kReady,
        kRunning,
        kHalted,
        kLoadError,
        kFetchOutOfBounds,
        kInvalidInstruction,
        kMemoryOutOfBounds,
    };
    
    struct StepResult{
        Status status;
        Word value = 0;

        bool halted(){
            return status == Status::kHalted;
        }

        bool failed(){
            return status == Status::kLoadError || status == Status::kFetchOutOfBounds || status == Status::kInvalidInstruction || status == Status::kMemoryOutOfBounds;
        }
    };

    Interpreter() = default;

    ImageLoadResult load(std::istream &input);// 装载新程序，同时重置 PC、寄存器和执行计数


    StepResult step();

    StepResult run();

    Status status() const noexcept{
        return status_;
    }

    const MemoryImage& memory() const{
        return memory_;
    }

    Address pc(){
        return pc_;
    }

    Word register_value(RegisterIndex idx){
        return idx < register_.size() ? register_[idx] : Word{0};
    }

    std::array<Word, 32>& registers(){
        return register_;
    }

    CycleCount instruction_count(){
        return instruction_count_;
    }

    Word exit_code(){
        return exit_code_;
    }
private:
    MemoryImage memory_;
    std::array<Word, 32> register_{};
    Status status_ = Status::kNotLoaded;

    Address pc_ = 0;
    CycleCount instruction_count_ = 0;
    Word exit_code_ = 0;

    StepResult current_result(){
        return {status_, exit_code_};
    }
}; 