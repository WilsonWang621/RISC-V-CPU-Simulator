#include <cstdlib>
#include <iostream>
#include <sstream>
#include <string>
#include <string_view>

#include "common/config.hpp"
#include "memory/memory_unit.hpp"
#include "test_log.hpp"
#include "test_support.hpp"

#ifndef MEMORY_UNIT_TEST_LOG_PATH
#define MEMORY_UNIT_TEST_LOG_PATH "riscv_memory_unit_tests.log"
#endif

namespace {

constexpr RobTag make_tag(
    RobIndex index,
    TagGeneration generation = 1U
) {
    return RobTag{index, generation, true};
}

DataMemoryRequest make_request(
    MemoryAccessType type,
    MemoryWidth width,
    Address address,
    RobTag tag,
    Word value = 0U
) {
    DataMemoryRequest request{};
    request.type = type;
    request.width = width;
    request.address = address;
    request.value = value;
    request.tag = tag;
    request.valid = true;
    return request;
}

ImageLoadResult load_text(MemoryUnit& memory, const std::string& text) {
    std::istringstream input(text);
    return memory.load(input);
}

void idle_cycle(MemoryUnit& memory) {
    EXPECT_FALSE(memory.apply(DataMemoryRequest{}));
    memory.latch();
}

void issue_and_finish(
    MemoryUnit& memory,
    const DataMemoryRequest& request
) {
    EXPECT_TRUE(memory.apply(request));
    memory.latch();

    for (std::size_t cycle = 0U; cycle < MemoryUnit::kDataLatency; ++cycle) {
        EXPECT_FALSE(memory.apply(DataMemoryRequest{}));
        memory.latch();
    }
}

void expect_tag(RobTag actual, RobTag expected) {
    EXPECT_EQ(actual.index, expected.index);
    EXPECT_EQ(actual.generation, expected.generation);
    EXPECT_EQ(actual.valid, expected.valid);
}

void expect_empty_response(const DataMemoryResponse& response) {
    EXPECT_FALSE(response.valid);
    EXPECT_FALSE(response.success);
    EXPECT_FALSE(response.is_store);
    EXPECT_FALSE(response.tag.valid);
    EXPECT_EQ(response.value, Word{0});
}

void test_constructor_starts_idle() {
    MemoryUnit memory;

    EXPECT_TRUE(memory.data_port_available());
    expect_empty_response(memory.data_response());

    EXPECT_FALSE(memory.apply(DataMemoryRequest{}));
    EXPECT_TRUE(memory.data_port_available());
    memory.latch();

    EXPECT_TRUE(memory.data_port_available());
    expect_empty_response(memory.data_response());
}

void test_instruction_fetch() {
    MemoryUnit memory;
    ImageLoadResult loaded = load_text(
        memory,
        "@00000020 13 05 f0 0f"
    );
    EXPECT_TRUE(loaded.ok());

    InstructionFetchRequest invalid{};
    const InstructionFetchResponse ignored = memory.fetch(invalid);
    EXPECT_FALSE(ignored.valid);
    EXPECT_FALSE(ignored.success);

    InstructionFetchRequest request{};
    request.pc = 0x20U;
    request.valid = true;
    const InstructionFetchResponse response = memory.fetch(request);
    EXPECT_TRUE(response.valid);
    EXPECT_TRUE(response.success);
    EXPECT_EQ(response.pc, Address{0x20U});
    EXPECT_EQ(response.instruction, InstructionBits{0x0ff00513U});

    request.pc = static_cast<Address>(kMemorySize - 1U);
    const InstructionFetchResponse out_of_bounds = memory.fetch(request);
    EXPECT_TRUE(out_of_bounds.valid);
    EXPECT_FALSE(out_of_bounds.success);
    EXPECT_EQ(out_of_bounds.pc, request.pc);
}

void test_request_latency_and_response_pulse() {
    MemoryUnit memory;
    EXPECT_TRUE(load_text(memory, "@00000010 78 56 34 12").ok());

    const RobTag tag = make_tag(3U, 7U);
    const DataMemoryRequest request = make_request(
        MemoryAccessType::LOAD,
        MemoryWidth::WORD,
        0x10U,
        tag
    );

    EXPECT_TRUE(memory.apply(request));
    expect_empty_response(memory.data_response());
    memory.latch();
    EXPECT_FALSE(memory.data_port_available());

    for (
        std::size_t cycle = 1U;
        cycle < MemoryUnit::kDataLatency;
        ++cycle
    ) {
        EXPECT_FALSE(memory.apply(DataMemoryRequest{}));
        expect_empty_response(memory.data_response());
        memory.latch();
        expect_empty_response(memory.data_response());
        EXPECT_FALSE(memory.data_port_available());
    }

    EXPECT_FALSE(memory.apply(DataMemoryRequest{}));
    expect_empty_response(memory.data_response());
    memory.latch();

    const DataMemoryResponse response = memory.data_response();
    EXPECT_TRUE(response.valid);
    EXPECT_TRUE(response.success);
    EXPECT_FALSE(response.is_store);
    EXPECT_EQ(response.value, Word{0x12345678U});
    expect_tag(response.tag, tag);
    EXPECT_TRUE(memory.data_port_available());

    idle_cycle(memory);
    expect_empty_response(memory.data_response());
}

void test_busy_port_rejects_second_request() {
    MemoryUnit memory;
    EXPECT_TRUE(load_text(memory, "@00000000 11 22 33 44").ok());

    const RobTag first_tag = make_tag(4U);
    const DataMemoryRequest first = make_request(
        MemoryAccessType::LOAD,
        MemoryWidth::BYTE,
        0U,
        first_tag
    );
    const DataMemoryRequest second = make_request(
        MemoryAccessType::LOAD,
        MemoryWidth::BYTE,
        1U,
        make_tag(5U)
    );

    EXPECT_TRUE(memory.apply(first));
    memory.latch();
    EXPECT_FALSE(memory.data_port_available());

    EXPECT_FALSE(memory.apply(second));
    memory.latch();
    EXPECT_FALSE(memory.apply(second));
    memory.latch();
    EXPECT_FALSE(memory.apply(second));
    memory.latch();

    const DataMemoryResponse response = memory.data_response();
    EXPECT_TRUE(response.valid);
    EXPECT_EQ(response.value, Word{0x11U});
    expect_tag(response.tag, first_tag);
}

void test_load_widths_return_raw_zero_extended_data() {
    MemoryUnit memory;
    EXPECT_TRUE(load_text(memory, "@00000040 ef cd ab 89").ok());

    const DataMemoryRequest byte_load = make_request(
        MemoryAccessType::LOAD,
        MemoryWidth::BYTE,
        0x40U,
        make_tag(6U)
    );
    issue_and_finish(memory, byte_load);
    EXPECT_TRUE(memory.data_response().success);
    EXPECT_EQ(memory.data_response().value, Word{0xefU});
    idle_cycle(memory);

    const DataMemoryRequest half_load = make_request(
        MemoryAccessType::LOAD,
        MemoryWidth::HALF,
        0x41U,
        make_tag(7U)
    );
    issue_and_finish(memory, half_load);
    EXPECT_TRUE(memory.data_response().success);
    EXPECT_EQ(memory.data_response().value, Word{0xabcdU});
    idle_cycle(memory);

    const DataMemoryRequest word_load = make_request(
        MemoryAccessType::LOAD,
        MemoryWidth::WORD,
        0x40U,
        make_tag(8U)
    );
    issue_and_finish(memory, word_load);
    EXPECT_TRUE(memory.data_response().success);
    EXPECT_EQ(memory.data_response().value, Word{0x89abcdefU});
}

void test_store_is_applied_only_on_completion_latch() {
    MemoryUnit memory;
    EXPECT_TRUE(load_text(memory, "@00000080 00 00 00 00").ok());

    const RobTag tag = make_tag(9U, 2U);
    const DataMemoryRequest store = make_request(
        MemoryAccessType::STORE,
        MemoryWidth::WORD,
        0x80U,
        tag,
        0x12345678U
    );

    EXPECT_TRUE(memory.apply(store));
    memory.latch();
    for (
        std::size_t cycle = 1U;
        cycle < MemoryUnit::kDataLatency;
        ++cycle
    ) {
        idle_cycle(memory);
    }

    InstructionFetchRequest fetch{};
    fetch.pc = 0x80U;
    fetch.valid = true;
    EXPECT_EQ(memory.fetch(fetch).instruction, InstructionBits{0U});

    EXPECT_FALSE(memory.apply(DataMemoryRequest{}));
    EXPECT_EQ(memory.fetch(fetch).instruction, InstructionBits{0U});
    memory.latch();

    const DataMemoryResponse response = memory.data_response();
    EXPECT_TRUE(response.valid);
    EXPECT_TRUE(response.success);
    EXPECT_TRUE(response.is_store);
    expect_tag(response.tag, tag);
    EXPECT_EQ(
        memory.fetch(fetch).instruction,
        InstructionBits{0x12345678U}
    );
}

void test_store_widths_mask_high_bits() {
    MemoryUnit memory;
    EXPECT_TRUE(load_text(memory, "@000000a0 11 22 33 44").ok());

    issue_and_finish(
        memory,
        make_request(
            MemoryAccessType::STORE,
            MemoryWidth::BYTE,
            0xa1U,
            make_tag(10U),
            0xabcdefeeU
        )
    );
    idle_cycle(memory);

    issue_and_finish(
        memory,
        make_request(
            MemoryAccessType::STORE,
            MemoryWidth::HALF,
            0xa2U,
            make_tag(11U),
            0x1234beefU
        )
    );

    InstructionFetchRequest fetch{};
    fetch.pc = 0xa0U;
    fetch.valid = true;
    const InstructionFetchResponse contents = memory.fetch(fetch);
    EXPECT_TRUE(contents.success);
    EXPECT_EQ(contents.instruction, InstructionBits{0xbeefee11U});
}

void test_out_of_bounds_data_access_reports_failure() {
    MemoryUnit memory;
    const Address last_address = static_cast<Address>(kMemorySize - 1U);

    issue_and_finish(
        memory,
        make_request(
            MemoryAccessType::LOAD,
            MemoryWidth::WORD,
            last_address,
            make_tag(12U)
        )
    );
    EXPECT_TRUE(memory.data_response().valid);
    EXPECT_FALSE(memory.data_response().success);
    EXPECT_FALSE(memory.data_response().is_store);
    EXPECT_EQ(memory.data_response().value, Word{0U});
    idle_cycle(memory);

    issue_and_finish(
        memory,
        make_request(
            MemoryAccessType::STORE,
            MemoryWidth::HALF,
            last_address,
            make_tag(13U),
            0xffffU
        )
    );
    EXPECT_TRUE(memory.data_response().valid);
    EXPECT_FALSE(memory.data_response().success);
    EXPECT_TRUE(memory.data_response().is_store);
}

void test_reset_and_load_cancel_in_flight_transaction() {
    MemoryUnit memory;
    EXPECT_TRUE(load_text(memory, "@000000c0 aa bb cc dd").ok());

    const DataMemoryRequest request = make_request(
        MemoryAccessType::LOAD,
        MemoryWidth::WORD,
        0xc0U,
        make_tag(14U)
    );
    EXPECT_TRUE(memory.apply(request));
    memory.latch();
    memory.reset();
    EXPECT_TRUE(memory.data_port_available());
    expect_empty_response(memory.data_response());

    InstructionFetchRequest fetch{};
    fetch.pc = 0xc0U;
    fetch.valid = true;
    EXPECT_EQ(memory.fetch(fetch).instruction, InstructionBits{0xddccbbaaU});

    EXPECT_TRUE(memory.apply(request));
    memory.latch();
    EXPECT_TRUE(load_text(memory, "@000000c0 01 02 03 04").ok());
    EXPECT_TRUE(memory.data_port_available());
    expect_empty_response(memory.data_response());
    EXPECT_EQ(memory.fetch(fetch).instruction, InstructionBits{0x04030201U});
}

void test_clear_erases_memory_and_pipeline_state() {
    MemoryUnit memory;
    EXPECT_TRUE(load_text(memory, "@000000e0 ff ff ff ff").ok());

    const DataMemoryRequest request = make_request(
        MemoryAccessType::LOAD,
        MemoryWidth::WORD,
        0xe0U,
        make_tag(15U)
    );
    EXPECT_TRUE(memory.apply(request));
    memory.latch();
    memory.clear();

    EXPECT_TRUE(memory.data_port_available());
    expect_empty_response(memory.data_response());

    InstructionFetchRequest fetch{};
    fetch.pc = 0xe0U;
    fetch.valid = true;
    const InstructionFetchResponse response = memory.fetch(fetch);
    EXPECT_TRUE(response.success);
    EXPECT_EQ(response.instruction, InstructionBits{0U});
}

}  // namespace

int main(int argc, char* argv[]) {
    test::TestLog log(MEMORY_UNIT_TEST_LOG_PATH);
    if (!log.is_open()) {
        std::cerr << "failed to open memory-unit test log: "
                  << MEMORY_UNIT_TEST_LOG_PATH << '\n';
        return EXIT_FAILURE;
    }
    std::cout << "log: " << MEMORY_UNIT_TEST_LOG_PATH << '\n';

    struct TestCase {
        std::string_view name;
        void (*run)();
    };

    const TestCase cases[]{
        {"initialization", test_constructor_starts_idle},
        {"fetch", test_instruction_fetch},
        {"latency", test_request_latency_and_response_pulse},
        {"busy-port", test_busy_port_rejects_second_request},
        {"load-widths", test_load_widths_return_raw_zero_extended_data},
        {"store-edge", test_store_is_applied_only_on_completion_latch},
        {"store-widths", test_store_widths_mask_high_bits},
        {"bounds", test_out_of_bounds_data_access_reports_failure},
        {"reset-load", test_reset_and_load_cancel_in_flight_transaction},
        {"clear", test_clear_erases_memory_and_pipeline_state},
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
        std::cerr << "unknown memory-unit test case: " << filter << '\n';
        std::cerr << "available cases:";
        for (const TestCase& test_case : cases) {
            std::cerr << ' ' << test_case.name;
        }
        std::cerr << '\n';
        return EXIT_FAILURE;
    }

    if (test::failure_count != 0) {
        std::cerr << test::failure_count << " memory-unit test(s) failed\n";
        return EXIT_FAILURE;
    }

    std::cout << "all memory-unit unit tests passed\n";
    return EXIT_SUCCESS;
}
