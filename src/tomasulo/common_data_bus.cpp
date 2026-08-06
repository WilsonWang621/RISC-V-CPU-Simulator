#include"common_data_bus.hpp"

CommonDataBus::CommonDataBus(){
    reset();
}

void CommonDataBus::reset(){
    prefer_load = false;
    next_prefer_load = false;
}

void CommonDataBus::latch(){
    prefer_load = next_prefer_load;
}

bool CommonDataBus::valid_candidate(const FUResult& result){
    if(!result.valid || !result.tag.valid){
        return false;
    }

    return true;
}

void CommonDataBus::select_integer(const FUResult& result, CDBOutputs& outputs){
    outputs.source = CDBSource::IntegerUnit;
    outputs.integer_granted = true;
    
    outputs.completion = result;
    outputs.broadcast.tag = result.tag;
    outputs.broadcast.value = result.result;
    outputs.broadcast.valid = true;
}

void CommonDataBus::select_load(const FUResult& result, CDBOutputs& outputs){
    outputs.source = CDBSource::LoadUnit;
    outputs.load_granted = true;

    outputs.completion = result;

    outputs.broadcast.tag = result.tag;
    outputs.broadcast.value = result.result;
    outputs.broadcast.valid = true;
}

CDBDecision CommonDataBus::plan(const CDBInputs& inputs) const{
    CDBDecision decision{};
    CDBOutputs& outputs = decision.outputs;

    decision.next_prefer_load = prefer_load;

    const bool integer_valid = valid_candidate(inputs.integer_result);
    const bool load_valid = valid_candidate(inputs.load_result);

    if (!integer_valid && !load_valid) {
        return decision;
    }

    if(integer_valid && load_valid){
        if(prefer_load){
            select_load(inputs.load_result, outputs);
            decision.next_prefer_load = false;
        }else{
            select_integer(inputs.integer_result, outputs);
            decision.next_prefer_load = true;
        }
        return decision;
    }

    if(integer_valid){
        select_integer(inputs.integer_result, outputs);
        decision.next_prefer_load = true;
        return decision;
    }

    select_load(inputs.load_result, outputs);
    decision.next_prefer_load = false;
    return decision;
}

void CommonDataBus::apply(const CDBDecision& decision){
    next_prefer_load = decision.next_prefer_load;
}
