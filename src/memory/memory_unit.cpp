#include"memory/memory_unit.hpp"

#include<cstddef>
#include<cassert>

MemoryUnit::MemoryUnit(){
    reset();
}

void MemoryUnit::reset(){
    current_transaction_ = DataTransaction{};
    next_transaction_ = DataTransaction{};

    current_response_ = DataMemoryResponse{};
    next_response_ = DataMemoryResponse{};

    next_store_write_ = PendingStoreWrite{};
}

void MemoryUnit::clear(){
    memory_.clear();
    reset();
}

ImageLoadResult MemoryUnit::load(std::istream& inputs){
    reset();
    return memory_.load(inputs);
}

InstructionFetchResponse MemoryUnit::fetch(const InstructionFetchRequest& request){
    InstructionFetchResponse response{};

    if (!request.valid) {
        return response;
    }

    response.pc = request.pc;
    response.valid = true;

    const auto instruction = memory_.read_word(request.pc);

    if(!instruction.has_value()){
        response.success = false;
        return response;
    }

    response.instruction = static_cast<InstructionBits>(*instruction);
    response.success = true;

    return response;
}

bool MemoryUnit::data_port_available() const{
    return !current_transaction_.valid;
}

DataMemoryResponse MemoryUnit::data_response() const{
    return current_response_;
}

std::size_t MemoryUnit::width_in_bytes(MemoryWidth width) {
    switch (width) {
        case MemoryWidth::BYTE:
            return sizeof(Byte);  //1

        case MemoryWidth::HALF:
            return sizeof(HalfWord);  //2

        case MemoryWidth::WORD:
            return sizeof(Word);   //4
    }

    return 0U;
}

Word MemoryUnit::read_data(Address address, MemoryWidth width, bool& success){
    success = false;

    switch (width) {
        case MemoryWidth::BYTE: {
            const auto value = memory_.read_byte(address);

            if (!value.has_value()) {
                return Word{0};
            }

            success = true;
            return static_cast<Word>(*value);
        }

        case MemoryWidth::HALF: {
            const auto value = memory_.read_half(address);

            if (!value.has_value()) {
                return Word{0};
            }

            success = true;
            return static_cast<Word>(*value);
        }

        case MemoryWidth::WORD: {
            const auto value = memory_.read_word(address);

            if (!value.has_value()) {
                return Word{0};
            }

            success = true;
            return *value;
        }
    }

    return Word{0};
}

bool MemoryUnit::schedule_store(const DataMemoryRequest& request){
    const size_t width = width_in_bytes(request.width);

    if(width == 0U || !memory_.in_bounds(request.address, width)){
        return false;
    }

    next_store_write_.address = request.address;
    next_store_write_.valid = true;
    next_store_write_.value = request.value;
    next_store_write_.width = request.width;

    return true;
}

void MemoryUnit::complete_current_transaction(){
    assert(current_transaction_.valid);

    const DataMemoryRequest &request = current_transaction_.request;

    DataMemoryResponse response{};
    response.tag = request.tag;
    response.is_store = request.type == MemoryAccessType::STORE;
    response.valid = true;

    if (request.type == MemoryAccessType::LOAD){
        response.value = read_data(request.address, request.width, response.success);
    }
    else{
        response.success = schedule_store(request);
    }
    
    next_response_ = response;
    next_transaction_ = DataTransaction{};
}

bool MemoryUnit::evaluate(const DataMemoryRequest& request){
    // 默认保持当前事务。
    next_transaction_ = current_transaction_;

    // Response 只保持一个周期。
    next_response_ = DataMemoryResponse{};

    // 每周期重新生成待写事件。
    next_store_write_ = PendingStoreWrite{};

    /*
    * 先推进当前事务。
    * 当前实现比较保守：事务完成的周期不同时接受新请求
    */
    if (current_transaction_.valid) {
        if (current_transaction_.remaining_cycles > 1U) {
            --next_transaction_.remaining_cycles;
        } else {
            complete_current_transaction();
        }
        return false;
    }

    if (!request.valid) {
        return false;
    }

    // 有效数据请求必须属于某个 ROB 项。
    if (!request.tag.valid) {
        assert(false);
        return false;
    }

    next_transaction_.request = request;
    next_transaction_.remaining_cycles = kDataLatency;

    next_transaction_.valid = true;
    return true;
}

bool MemoryUnit::apply_store_write(const PendingStoreWrite& write){
    if (!write.valid) {
        return true;
    }

    switch (write.width) {
        case MemoryWidth::BYTE:
            return memory_.write_byte( write.address,static_cast<Byte>(write.value & 0xffU));

        case MemoryWidth::HALF:
            return memory_.write_half(write.address,static_cast<HalfWord>(write.value & 0xffffU));

        case MemoryWidth::WORD:
            return memory_.write_word( write.address,write.value);
    }

    return false;
}

void MemoryUnit::latch(){
    // Store 在时钟边沿真正修改内存。
    if (next_store_write_.valid) {
        const bool written = apply_store_write(next_store_write_);

        // evaluate 已经检查过边界，这里不应失败。
        assert(written);
    }

    current_transaction_ = next_transaction_;

    current_response_ = next_response_;
}