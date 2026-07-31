cmake_minimum_required(VERSION 3.16)

project(
    riscv_tomasulo_simulator
    VERSION 0.1.0
    DESCRIPTION "RV32I Tomasulo CPU simulator"
    LANGUAGES CXX
)

option(RISCV_ENABLE_WARNINGS "Enable additional compiler warnings" ON)

add_library(riscv_project_options INTERFACE)
target_compile_features(riscv_project_options INTERFACE cxx_std_17)
target_include_directories(
    riscv_project_options
    INTERFACE
        "$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>"
)

if(RISCV_ENABLE_WARNINGS)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(
            riscv_project_options
            INTERFACE
                -Wall
                -Wextra
                -Wpedantic
                -Wconversion
                -Wshadow
        )
    elseif(MSVC)
        target_compile_options(riscv_project_options INTERFACE /W4 /permissive-)
    endif()
endif()

add_library(
    riscv_core
    src/isa/decoder.cpp
    src/memory/image.cpp
    src/reference/interpreter.cpp
    src/tomasulo/issue_unit.cpp
    src/tomasulo/register_file.cpp
    src/tomasulo/rename_table.cpp
)
target_link_libraries(riscv_core PUBLIC riscv_project_options)
target_include_directories(
    riscv_core
    PRIVATE
        "${PROJECT_SOURCE_DIR}/include/memory"
        "${PROJECT_SOURCE_DIR}/include/reference"
        "${PROJECT_SOURCE_DIR}/include/tomasulo"
        "${PROJECT_SOURCE_DIR}/include/common"
)

add_executable(riscv_simulator src/main.cpp)
target_link_libraries(riscv_simulator PRIVATE riscv_core)

include(CTest)
if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
