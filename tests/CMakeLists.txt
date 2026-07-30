add_executable(
    riscv_unit_tests
    test_main.cpp
    unit/decoder_test.cpp
    unit/memory_test.cpp
    unit/queue_test.cpp
    unit/types_test.cpp
)

target_link_libraries(riscv_unit_tests PRIVATE riscv_core)
target_include_directories(
    riscv_unit_tests
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}"
)

add_test(NAME riscv_unit_tests COMMAND riscv_unit_tests)

add_executable(
    riscv_issue_unit_tests
    unit/issue_unit_test.cpp
)

target_link_libraries(riscv_issue_unit_tests PRIVATE riscv_core)
target_include_directories(
    riscv_issue_unit_tests
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}"
)
target_compile_definitions(
    riscv_issue_unit_tests
    PRIVATE
        ISSUE_TEST_LOG_PATH="${CMAKE_CURRENT_BINARY_DIR}/riscv_issue_unit_tests.log"
)

add_test(NAME riscv_issue_unit_tests COMMAND riscv_issue_unit_tests)

add_executable(
    riscv_interpreter_data_tests
    unit/interpreter_data_test.cpp
)

target_link_libraries(riscv_interpreter_data_tests PRIVATE riscv_core)
target_compile_definitions(
    riscv_interpreter_data_tests
    PRIVATE
        RISCV_SOURCE_DIR="${PROJECT_SOURCE_DIR}"
)

add_test(
    NAME riscv_interpreter_data_tests
    COMMAND riscv_interpreter_data_tests
)
set_tests_properties(
    riscv_interpreter_data_tests
    PROPERTIES
        LABELS "interpreter;data"
        TIMEOUT 180
)
