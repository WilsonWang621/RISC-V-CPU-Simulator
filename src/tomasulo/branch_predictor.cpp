#include"tomasulo/branch_predictor.hpp"

BranchPredictor::BranchPredictor(){
    reset();
}

size_t BranchPredictor::index_of(Address pc){
    return static_cast<size_t>((pc >> 2U) & static_cast<Address>(kTableSize - 1U));
}

bool BranchPredictor::predict_taken(Address pc)const{
    return current_[index_of(pc)] >= kWeakTaken;
}

std::uint8_t BranchPredictor::counter(Address pc)const{
    return current_[index_of(pc)];
}

void BranchPredictor::apply(const BranchPredictorUpdate& update){
    next_ = current_;
    next_prediction_count_ = prediction_count_;
    next_correct_count_ = correct_count_;
    
    if(!update.valid){
        return;
    }

    std::uint8_t& state = next_[index_of(update.pc)];

    if (update.actual_taken) {
        if (state < kStrongTaken) {
            ++state;
        }
    } else {
        if (state > kStrongNotTaken) {
            --state;
        }
    }

    ++next_prediction_count_;

    if (update.predicted_next_pc ==
        update.actual_next_pc) {
        ++next_correct_count_;
    }
}

void BranchPredictor::latch(){
    current_ = next_;
    prediction_count_ = next_prediction_count_;
    correct_count_ = next_correct_count_;
}

void BranchPredictor::reset() {
    current_.fill(kWeakNotTaken);
    next_.fill(kWeakNotTaken);

    prediction_count_ = 0;
    next_prediction_count_ = 0;

    correct_count_ = 0;
    next_correct_count_ = 0;
}

CycleCount BranchPredictor::prediction_count() const {
    return prediction_count_;
}

CycleCount BranchPredictor::correct_count() const {
    return correct_count_;
}

double BranchPredictor::accuracy() const{
    if (prediction_count_ == 0U) {
        return 0.0;
    }

    return static_cast<double>(correct_count_) / static_cast<double>(prediction_count_);
}
