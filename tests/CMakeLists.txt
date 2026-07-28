add_executable(
    riscv_unit_tests
    test_main.cpp
    unit/queue_test.cpp
    unit/types_test.cpp
)

target_link_libraries(riscv_unit_tests PRIVATE riscv_project_options)
target_include_directories(
    riscv_unit_tests
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}"
)

add_test(NAME riscv_unit_tests COMMAND riscv_unit_tests)
