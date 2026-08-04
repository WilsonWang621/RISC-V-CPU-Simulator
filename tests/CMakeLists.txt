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
    riscv_register_file_unit_tests
    unit/register_file_test.cpp
)

target_link_libraries(riscv_register_file_unit_tests PRIVATE riscv_core)
target_include_directories(
    riscv_register_file_unit_tests
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}"
)
target_compile_definitions(
    riscv_register_file_unit_tests
    PRIVATE
        REGISTER_FILE_TEST_LOG_PATH="${CMAKE_CURRENT_BINARY_DIR}/riscv_register_file_unit_tests.log"
)

add_test(
    NAME riscv_register_file_unit_tests
    COMMAND riscv_register_file_unit_tests
)

add_executable(
    riscv_rename_table_unit_tests
    unit/rename_table_test.cpp
)

target_link_libraries(riscv_rename_table_unit_tests PRIVATE riscv_core)
target_include_directories(
    riscv_rename_table_unit_tests
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}"
        "${PROJECT_SOURCE_DIR}/include/common"
)
target_compile_definitions(
    riscv_rename_table_unit_tests
    PRIVATE
        RENAME_TABLE_TEST_LOG_PATH="${CMAKE_CURRENT_BINARY_DIR}/riscv_rename_table_unit_tests.log"
)

add_test(
    NAME riscv_rename_table_unit_tests
    COMMAND riscv_rename_table_unit_tests
)

add_executable(
    riscv_reservation_station_unit_tests
    unit/reservation_station_test.cpp
)

target_link_libraries(
    riscv_reservation_station_unit_tests
    PRIVATE
        riscv_core
)
target_include_directories(
    riscv_reservation_station_unit_tests
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}"
)
target_compile_definitions(
    riscv_reservation_station_unit_tests
    PRIVATE
        RESERVATION_STATION_TEST_LOG_PATH="${CMAKE_CURRENT_BINARY_DIR}/riscv_reservation_station_unit_tests.log"
)

add_test(
    NAME riscv_reservation_station_unit_tests
    COMMAND riscv_reservation_station_unit_tests
)

add_executable(
    riscv_alu_unit_tests
    unit/functional_unit_test.cpp
)

target_link_libraries(riscv_alu_unit_tests PRIVATE riscv_core)
target_include_directories(
    riscv_alu_unit_tests
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}"
)
target_compile_definitions(
    riscv_alu_unit_tests
    PRIVATE
        ALU_TEST_LOG_PATH="${CMAKE_CURRENT_BINARY_DIR}/riscv_alu_unit_tests.log"
)

add_test(NAME riscv_alu_unit_tests COMMAND riscv_alu_unit_tests)

add_executable(
    riscv_rob_unit_tests
    unit/reorder_buffer_test.cpp
)

target_link_libraries(riscv_rob_unit_tests PRIVATE riscv_core)
target_include_directories(
    riscv_rob_unit_tests
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}"
)
target_compile_definitions(
    riscv_rob_unit_tests
    PRIVATE
        ROB_TEST_LOG_PATH="${CMAKE_CURRENT_BINARY_DIR}/riscv_rob_unit_tests.log"
)

add_test(NAME riscv_rob_unit_tests COMMAND riscv_rob_unit_tests)

add_executable(
    riscv_cdb_unit_tests
    unit/common_data_bus_test.cpp
)

target_link_libraries(riscv_cdb_unit_tests PRIVATE riscv_core)
target_include_directories(
    riscv_cdb_unit_tests
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}"
)
target_compile_definitions(
    riscv_cdb_unit_tests
    PRIVATE
        CDB_TEST_LOG_PATH="${CMAKE_CURRENT_BINARY_DIR}/riscv_cdb_unit_tests.log"
)

add_test(NAME riscv_cdb_unit_tests COMMAND riscv_cdb_unit_tests)

add_executable(
    riscv_lsq_unit_tests
    unit/load_store_queue_test.cpp
)

target_link_libraries(riscv_lsq_unit_tests PRIVATE riscv_core)
target_include_directories(
    riscv_lsq_unit_tests
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}"
)
target_compile_definitions(
    riscv_lsq_unit_tests
    PRIVATE
        LSQ_TEST_LOG_PATH="${CMAKE_CURRENT_BINARY_DIR}/riscv_lsq_unit_tests.log"
)

add_test(NAME riscv_lsq_unit_tests COMMAND riscv_lsq_unit_tests)

add_executable(
    riscv_memory_unit_tests
    unit/memory_unit_test.cpp
)

target_link_libraries(riscv_memory_unit_tests PRIVATE riscv_core)
target_include_directories(
    riscv_memory_unit_tests
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}"
)
target_compile_definitions(
    riscv_memory_unit_tests
    PRIVATE
        MEMORY_UNIT_TEST_LOG_PATH="${CMAKE_CURRENT_BINARY_DIR}/riscv_memory_unit_tests.log"
)

add_test(NAME riscv_memory_unit_tests COMMAND riscv_memory_unit_tests)

add_executable(
    riscv_branch_predictor_unit_tests
    unit/branch_predictor_test.cpp
)

target_link_libraries(riscv_branch_predictor_unit_tests PRIVATE riscv_core)
target_include_directories(
    riscv_branch_predictor_unit_tests
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}"
)
target_compile_definitions(
    riscv_branch_predictor_unit_tests
    PRIVATE
        BRANCH_PREDICTOR_TEST_LOG_PATH="${CMAKE_CURRENT_BINARY_DIR}/riscv_branch_predictor_unit_tests.log"
)

add_test(
    NAME riscv_branch_predictor_unit_tests
    COMMAND riscv_branch_predictor_unit_tests
)

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
