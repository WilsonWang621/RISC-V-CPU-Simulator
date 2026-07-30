#include"register_file.hpp"

RegisterFile::RegisterFile(){
    reset();
}

void RegisterFile::reset(){
    register_.fill(Word{0});
    pending_write_ = RegisterWrite{};
}

Word RegisterFile::read(RegisterIndex idx){
    const std::size_t pos = static_cast<std::size_t>(idx);
    if(pos == 0U || pos >= kRegisterCount){
        return Word{0};
    }

    return register_[pos];
}

const std::array<Word, kRegisterCount>& RegisterFile::values(){
    return register_;
}

void RegisterFile::evaluate_commit(const RegisterWrite& write){
    // 每周期都覆盖 pending_write_。
    // 即使本周期没有提交，也要传入 valid == false 的写请求，
    // 避免上一周期请求被重复执行。
    pending_write_ = write;

    const std::size_t position = static_cast<std::size_t>(pending_write_.rd);

    // 对 x0 或非法寄存器的写入直接作废
    if (position == 0U || position >= kRegisterCount) {
        pending_write_.valid = false;
    }
}

void RegisterFile::latch(){
    if (pending_write_.valid) {
        const std::size_t position = static_cast<std::size_t>(pending_write_.rd);
        register_[position] = pending_write_.value;
    }

    // 双重保证 x0 永远是 0。
    register_[0] = Word{0};

    // 写请求只能生效一次。
    pending_write_ = RegisterWrite{};
}