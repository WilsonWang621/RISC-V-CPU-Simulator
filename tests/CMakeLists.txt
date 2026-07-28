add_executable(
    riscv_unit_tests
    test_main.cpp
)

target_link_libraries(riscv_unit_tests PRIVATE riscv_project_options)

add_test(NAME riscv_unit_tests COMMAND riscv_unit_tests)
