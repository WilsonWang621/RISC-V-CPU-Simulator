cmake_minimum_required(VERSION 3.16)

project(
    riscv_tomasulo_simulator
    VERSION 0.1.0
    DESCRIPTION "RV32I Tomasulo CPU simulator"
    LANGUAGES CXX
)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

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

file(
    GLOB_RECURSE RISCV_CORE_SOURCES
    CONFIGURE_DEPENDS
    "${PROJECT_SOURCE_DIR}/src/*.cpp"
)
list(REMOVE_ITEM RISCV_CORE_SOURCES "${PROJECT_SOURCE_DIR}/src/main.cpp")

add_library(riscv_core ${RISCV_CORE_SOURCES})
target_link_libraries(riscv_core PUBLIC riscv_project_options)
target_include_directories(
    riscv_core
    PUBLIC
        "${PROJECT_SOURCE_DIR}/include/memory"
        "${PROJECT_SOURCE_DIR}/include/reference"
        "${PROJECT_SOURCE_DIR}/include/tomasulo"
        "${PROJECT_SOURCE_DIR}/include/common"
        "${PROJECT_SOURCE_DIR}/include/isa"
)

add_executable(code src/main.cpp)
target_link_libraries(code PRIVATE riscv_core)
set_target_properties(
    code
    PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${PROJECT_SOURCE_DIR}"
)

include(CTest)
if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
