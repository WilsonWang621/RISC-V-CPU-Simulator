#include <cstdlib>
#include <iostream>
#include <string_view>

#include "test_support.hpp"
#include "tomasulo/issue_unit.hpp"

namespace {

constexpr RobTag make_tag(RobIndex index, TagGeneration generation = 1U) {
    return RobTag{index, generation, true};
}

IssueInputs make_available_inputs(const DecodedInstruction& decoded) {
    IssueInputs inputs{};
    inputs.packet.pc = 0x1000U;
    inputs.packet.predicted_pc = 0x1004U;
    inputs.packet.decoded = decoded;
    inputs.packet.valid = true;
    inputs.allocated_tag = make_tag(3U, 2U);
    inputs.rob_available = true;
    inputs.rs_available = true;
    inputs.lsq_available = true;
    return inputs;
}

DecodedInstruction make_add() {
    DecodedInstruction decoded{};
    decoded.op = OP::ADD;
    decoded.rd = 5U;
    decoded.rs1 = 1U;
    decoded.rs2 = 2U;
    decoded.lhs_source = OperandSource::Register;
    decoded.rhs_source = OperandSource::Register;
    decoded.uses_rs1 = true;
    decoded.uses_rs2 = true;
    decoded.writes_rd = true;
    return decoded;
}

DecodedInstruction make_addi() {
    DecodedInstruction decoded{};
    decoded.op = OP::ADDI;
    decoded.rd = 5U;
    decoded.rs1 = 1U;
    decoded.immediate = 12U;
    decoded.lhs_source = OperandSource::Register;
    decoded.rhs_source = OperandSource::Immediate;
    decoded.uses_rs1 = true;
    decoded.writes_rd = true;
    return decoded;
}

void expect_no_writes(const IssueOutputs& outputs) {
    EXPECT_FALSE(outputs.pop_decode);
    EXPECT_FALSE(outputs.write_rob);
    EXPECT_FALSE(outputs.write_rs);
    EXPECT_FALSE(outputs.write_lsq);
    EXPECT_FALSE(outputs.rename.valid);
}

void expect_ready_operand(const Operand& operand, Word value) {
    EXPECT_TRUE(operand.ready);
    EXPECT_EQ(operand.value, value);
    EXPECT_FALSE(operand.tag.valid);
}

void expect_tag(RobTag actual, RobTag expected) {
    EXPECT_EQ(actual.index, expected.index);
    EXPECT_EQ(actual.generation, expected.generation);
    EXPECT_EQ(actual.valid, expected.valid);
}

void expect_waiting_operand(const Operand& operand, RobTag tag) {
    EXPECT_FALSE(operand.ready);
    expect_tag(operand.tag, tag);
}

void test_invalid_inputs_do_not_issue() {
    IssueUnit issue;

    IssueInputs empty{};
    const IssueOutputs empty_outputs = issue.evaluate(empty);
    EXPECT_EQ(empty_outputs.status, IssueStatus::Empty);
    expect_no_writes(empty_outputs);

    IssueInputs invalid_instruction = make_available_inputs(DecodedInstruction{});
    const IssueOutputs invalid_outputs = issue.evaluate(invalid_instruction);
    EXPECT_EQ(invalid_outputs.status, IssueStatus::InvalidInstruction);
    expect_no_writes(invalid_outputs);

    IssueInputs invalid_tag = make_available_inputs(make_add());
    invalid_tag.allocated_tag = RobTag{};
    const IssueOutputs invalid_tag_outputs = issue.evaluate(invalid_tag);
    EXPECT_EQ(invalid_tag_outputs.status, IssueStatus::InvalidAllocation);
    expect_no_writes(invalid_tag_outputs);
}

void test_required_resource_backpressure() {
    IssueUnit issue;

    IssueInputs no_rob = make_available_inputs(make_add());
    no_rob.rob_available = false;
    const IssueOutputs no_rob_outputs = issue.evaluate(no_rob);
    EXPECT_EQ(no_rob_outputs.status, IssueStatus::Unavailability);
    expect_no_writes(no_rob_outputs);

    IssueInputs no_rs = make_available_inputs(make_add());
    no_rs.rs_available = false;
    const IssueOutputs no_rs_outputs = issue.evaluate(no_rs);
    EXPECT_EQ(no_rs_outputs.status, IssueStatus::Unavailability);
    expect_no_writes(no_rs_outputs);

    DecodedInstruction load{};
    load.op = OP::LW;
    load.rd = 5U;
    load.rs1 = 1U;
    load.lhs_source = OperandSource::Register;
    load.rhs_source = OperandSource::Immediate;
    load.writes_rd = true;
    load.is_load = true;

    IssueInputs no_lsq = make_available_inputs(load);
    no_lsq.lsq_available = false;
    const IssueOutputs no_lsq_outputs = issue.evaluate(no_lsq);
    EXPECT_EQ(no_lsq_outputs.status, IssueStatus::Unavailability);
    expect_no_writes(no_lsq_outputs);

    IssueInputs load_without_rs = make_available_inputs(load);
    load_without_rs.rs_available = false;
    const IssueOutputs load_outputs = issue.evaluate(load_without_rs);
    EXPECT_TRUE(load_outputs.issued());
    EXPECT_TRUE(load_outputs.write_rob);
    EXPECT_TRUE(load_outputs.write_lsq);
    EXPECT_FALSE(load_outputs.write_rs);
}

void test_alu_issue_builds_rob_rs_and_rename_entries() {
    IssueUnit issue;
    IssueInputs inputs = make_available_inputs(make_add());
    inputs.rs1.architectural_value = 10U;
    inputs.rs2.architectural_value = 20U;

    const IssueOutputs outputs = issue.evaluate(inputs);

    EXPECT_TRUE(outputs.issued());
    EXPECT_TRUE(outputs.pop_decode);
    EXPECT_TRUE(outputs.write_rob);
    EXPECT_TRUE(outputs.write_rs);
    EXPECT_FALSE(outputs.write_lsq);

    EXPECT_TRUE(outputs.rob_entry.valid);
    expect_tag(outputs.rob_entry.tag, inputs.allocated_tag);
    EXPECT_EQ(outputs.rob_entry.op, OP::ADD);
    EXPECT_EQ(outputs.rob_entry.pc, inputs.packet.pc);
    EXPECT_EQ(outputs.rob_entry.rd, RegisterIndex{5U});
    EXPECT_TRUE(outputs.rob_entry.writes_rd);
    EXPECT_FALSE(outputs.rob_entry.ready);
    EXPECT_EQ(
        outputs.rob_entry.predicted_next_pc,
        inputs.packet.predicted_pc
    );

    EXPECT_TRUE(outputs.rs_entry.busy);
    EXPECT_EQ(outputs.rs_entry.op, OP::ADD);
    expect_tag(outputs.rs_entry.destination, inputs.allocated_tag);
    expect_ready_operand(outputs.rs_entry.lhs, 10U);
    expect_ready_operand(outputs.rs_entry.rhs, 20U);

    EXPECT_TRUE(outputs.rename.valid);
    EXPECT_EQ(outputs.rename.rd, RegisterIndex{5U});
    expect_tag(outputs.rename.tag, inputs.allocated_tag);
}

void test_operand_sources_and_zero_register() {
    IssueUnit issue;

    IssueInputs addi = make_available_inputs(make_addi());
    addi.rs1.architectural_value = 7U;
    const IssueOutputs addi_outputs = issue.evaluate(addi);
    expect_ready_operand(addi_outputs.rs_entry.lhs, 7U);
    expect_ready_operand(addi_outputs.rs_entry.rhs, 12U);

    DecodedInstruction auipc{};
    auipc.op = OP::AUIPC;
    auipc.rd = 6U;
    auipc.immediate = 0x2000U;
    auipc.lhs_source = OperandSource::ProgramCounter;
    auipc.rhs_source = OperandSource::Immediate;
    auipc.writes_rd = true;
    const IssueOutputs auipc_outputs =
        issue.evaluate(make_available_inputs(auipc));
    expect_ready_operand(auipc_outputs.rs_entry.lhs, 0x1000U);
    expect_ready_operand(auipc_outputs.rs_entry.rhs, 0x2000U);

    DecodedInstruction add_with_x0 = make_add();
    add_with_x0.rs1 = 0U;
    IssueInputs x0_inputs = make_available_inputs(add_with_x0);
    x0_inputs.rs1.producer = make_tag(9U);
    x0_inputs.rs1.producer_ready = false;
    x0_inputs.rs1.architectural_value = 0xdeadbeefU;
    const IssueOutputs x0_outputs = issue.evaluate(x0_inputs);
    expect_ready_operand(x0_outputs.rs_entry.lhs, 0U);
}

void test_register_dependency_resolution() {
    IssueUnit issue;
    const RobTag producer = make_tag(7U, 4U);

    IssueInputs waiting = make_available_inputs(make_add());
    waiting.rs1.producer = producer;
    waiting.rs1.producer_ready = false;
    const IssueOutputs waiting_outputs = issue.evaluate(waiting);
    expect_waiting_operand(waiting_outputs.rs_entry.lhs, producer);

    IssueInputs rob_ready = make_available_inputs(make_add());
    rob_ready.rs1.producer = producer;
    rob_ready.rs1.producer_ready = true;
    rob_ready.rs1.producer_value = 0x12345678U;
    const IssueOutputs rob_ready_outputs = issue.evaluate(rob_ready);
    expect_ready_operand(rob_ready_outputs.rs_entry.lhs, 0x12345678U);

    IssueInputs cdb_bypass = rob_ready;
    cdb_bypass.cdb.valid = true;
    cdb_bypass.cdb.tag = producer;
    cdb_bypass.cdb.value = 0xaabbccddU;
    const IssueOutputs cdb_outputs = issue.evaluate(cdb_bypass);
    expect_ready_operand(cdb_outputs.rs_entry.lhs, 0xaabbccddU);

    IssueInputs unrelated_cdb = waiting;
    unrelated_cdb.cdb.valid = true;
    unrelated_cdb.cdb.tag = make_tag(8U, 4U);
    unrelated_cdb.cdb.value = 99U;
    const IssueOutputs unrelated_outputs = issue.evaluate(unrelated_cdb);
    expect_waiting_operand(unrelated_outputs.rs_entry.lhs, producer);
}

void test_load_and_store_issue_to_lsq() {
    IssueUnit issue;

    DecodedInstruction load{};
    load.op = OP::LW;
    load.rd = 4U;
    load.rs1 = 1U;
    load.immediate = 16U;
    load.lhs_source = OperandSource::Register;
    load.rhs_source = OperandSource::Immediate;
    load.uses_rs1 = true;
    load.writes_rd = true;
    load.is_load = true;
    IssueInputs load_inputs = make_available_inputs(load);
    load_inputs.rs1.architectural_value = 0x2000U;

    const IssueOutputs load_outputs = issue.evaluate(load_inputs);
    EXPECT_TRUE(load_outputs.issued());
    EXPECT_TRUE(load_outputs.write_rob);
    EXPECT_TRUE(load_outputs.write_lsq);
    EXPECT_FALSE(load_outputs.write_rs);
    EXPECT_EQ(load_outputs.lsq_entry.type, LSType::LOAD);
    EXPECT_EQ(load_outputs.lsq_entry.op, OP::LW);
    expect_tag(
        load_outputs.lsq_entry.destination,
        load_inputs.allocated_tag
    );
    expect_ready_operand(load_outputs.lsq_entry.base, 0x2000U);
    expect_ready_operand(load_outputs.lsq_entry.store_data, 0U);
    EXPECT_EQ(load_outputs.lsq_entry.immediate, 16U);
    EXPECT_FALSE(load_outputs.lsq_entry.address_ready);
    EXPECT_FALSE(load_outputs.lsq_entry.request_sent);
    EXPECT_TRUE(load_outputs.lsq_entry.valid);
    EXPECT_TRUE(load_outputs.rename.valid);

    DecodedInstruction store{};
    store.op = OP::SW;
    store.rs1 = 1U;
    store.rs2 = 2U;
    store.immediate = 20U;
    store.lhs_source = OperandSource::Register;
    store.rhs_source = OperandSource::Register;
    store.uses_rs1 = true;
    store.uses_rs2 = true;
    store.is_store = true;
    IssueInputs store_inputs = make_available_inputs(store);
    store_inputs.rs1.architectural_value = 0x3000U;
    store_inputs.rs2.architectural_value = 0x55667788U;

    const IssueOutputs store_outputs = issue.evaluate(store_inputs);
    EXPECT_TRUE(store_outputs.issued());
    EXPECT_TRUE(store_outputs.write_rob);
    EXPECT_TRUE(store_outputs.write_lsq);
    EXPECT_FALSE(store_outputs.write_rs);
    EXPECT_EQ(store_outputs.lsq_entry.type, LSType::STORE);
    expect_ready_operand(store_outputs.lsq_entry.base, 0x3000U);
    expect_ready_operand(store_outputs.lsq_entry.store_data, 0x55667788U);
    EXPECT_TRUE(store_outputs.rob_entry.is_store);
    EXPECT_FALSE(store_outputs.rename.valid);
}

void test_branch_metadata_and_halt() {
    IssueUnit issue;

    DecodedInstruction branch{};
    branch.op = OP::BEQ;
    branch.rs1 = 1U;
    branch.rs2 = 2U;
    branch.lhs_source = OperandSource::Register;
    branch.rhs_source = OperandSource::Register;
    branch.uses_rs1 = true;
    branch.uses_rs2 = true;
    branch.is_branch = true;
    const IssueOutputs branch_outputs =
        issue.evaluate(make_available_inputs(branch));
    EXPECT_TRUE(branch_outputs.write_rs);
    EXPECT_TRUE(branch_outputs.rob_entry.is_branch);
    EXPECT_EQ(branch_outputs.rob_entry.predicted_next_pc, 0x1004U);
    EXPECT_FALSE(branch_outputs.rename.valid);

    DecodedInstruction halt{};
    halt.op = OP::HALT;
    IssueInputs halt_inputs = make_available_inputs(halt);
    halt_inputs.rs_available = false;
    halt_inputs.lsq_available = false;
    const IssueOutputs halt_outputs = issue.evaluate(halt_inputs);
    EXPECT_TRUE(halt_outputs.issued());
    EXPECT_TRUE(halt_outputs.pop_decode);
    EXPECT_TRUE(halt_outputs.write_rob);
    EXPECT_FALSE(halt_outputs.write_rs);
    EXPECT_FALSE(halt_outputs.write_lsq);
    EXPECT_TRUE(halt_outputs.rob_entry.ready);
    EXPECT_FALSE(halt_outputs.rename.valid);
}

void test_x0_is_never_a_rename_destination() {
    IssueUnit issue;
    DecodedInstruction decoded = make_addi();
    decoded.rd = 0U;

    // Issue should enforce the architectural x0 rule at its own output
    // boundary, even if upstream metadata incorrectly says writes_rd.
    decoded.writes_rd = true;

    const IssueOutputs outputs =
        issue.evaluate(make_available_inputs(decoded));

    EXPECT_TRUE(outputs.issued());
    EXPECT_FALSE(outputs.rob_entry.writes_rd);
    EXPECT_FALSE(outputs.rename.valid);
}

}  // namespace

int main(int argc, char* argv[]) {
    struct TestCase {
        std::string_view name;
        void (*run)();
    };

    const TestCase cases[]{
        {"invalid", test_invalid_inputs_do_not_issue},
        {"backpressure", test_required_resource_backpressure},
        {"alu", test_alu_issue_builds_rob_rs_and_rename_entries},
        {"operands", test_operand_sources_and_zero_register},
        {"dependency", test_register_dependency_resolution},
        {"lsq", test_load_and_store_issue_to_lsq},
        {"control", test_branch_metadata_and_halt},
        {"x0", test_x0_is_never_a_rename_destination},
    };

    const std::string_view filter = argc > 1 ? argv[1] : "";
    bool ran_test = false;
    for (const TestCase& test_case : cases) {
        if (!filter.empty() && filter != test_case.name) {
            continue;
        }
        ran_test = true;
        std::cout << "[ RUN      ] " << test_case.name << '\n';
        test_case.run();
    }

    if (!ran_test) {
        std::cerr << "unknown issue test case: " << filter << '\n';
        std::cerr << "available cases:";
        for (const TestCase& test_case : cases) {
            std::cerr << ' ' << test_case.name;
        }
        std::cerr << '\n';
        return EXIT_FAILURE;
    }

    if (test::failure_count != 0) {
        std::cerr << test::failure_count << " issue test(s) failed\n";
        return EXIT_FAILURE;
    }

    std::cout << "all issue unit tests passed\n";
    return EXIT_SUCCESS;
}
