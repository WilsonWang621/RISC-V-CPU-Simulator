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

CDBOutputs CommonDataBus::evaluate(const CDBInputs& inputs){
    CDBOutputs outputs{};

    next_prefer_load = prefer_load;

    const bool integer_valid = valid_candidate(inputs.integer_result);
    const bool load_valid = valid_candidate(inputs.load_result);

    if (!integer_valid && !load_valid) {
        return outputs;
    }

    if(integer_valid && load_valid){
        if(prefer_load){
            select_load(inputs.load_result, outputs);
            next_prefer_load = false;
        }else{
            select_integer(inputs.integer_result, outputs);
            next_prefer_load = true;
        }
        return outputs;
    }

    if(integer_valid){
        select_integer(inputs.integer_result, outputs);
        next_prefer_load = true;
        return outputs;
    }

    select_load(inputs.load_result, outputs);
    next_prefer_load = false;
    return outputs;
}