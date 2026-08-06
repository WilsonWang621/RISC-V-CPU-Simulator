#pragma once

#include"./signals.hpp"

enum class CDBSource{
    None, 
    IntegerUnit,
    LoadUnit
};
struct CDBInputs{
    // Integer FU 当前保存的完成结果。
    FUResult integer_result{};

    // 未来由 Load Unit 或 LSQ 提供。
    // Load 只需要填写 tag、result 和 valid。
    FUResult load_result{};
};

struct CDBOutputs{
    // 给 Issue、RS、LSQ 的普通广播。
    CDBMsg broadcast{};
    // 给 ROB 的完整完成信息。
    FUResult completion{};

    CDBSource source = CDBSource::None;

    // 生产者只有获得 grant 后才能删除当前结果。
    bool integer_granted = false;
    bool load_granted = false;
};

// 本周期仲裁的完整、不可变决策。plan() 只读取 current state；
// apply() 在所有模块输出都冻结后再提交 next state。
struct CDBDecision {
    CDBOutputs outputs{};
    bool next_prefer_load = false;
};

class CommonDataBus{
public:
    CommonDataBus();

    CDBDecision plan(const CDBInputs& inputs) const;
    void apply(const CDBDecision& decision);

    void reset();
    void latch();

private:
    //如果出现CDB竞争的情况，使用轮询防止Load或者Integer FU长时间饥饿
    bool prefer_load = false;
    bool next_prefer_load = false;

    static bool valid_candidate(const FUResult &result);

    static void select_integer(const FUResult& result, CDBOutputs& outputs);

    static void select_load(const FUResult& result, CDBOutputs& outputs);
};
