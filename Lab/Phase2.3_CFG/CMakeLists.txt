cmake_minimum_required(VERSION 3.15)
project(Phase2.3_CFG LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 包含目录
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

# 测试可执行文件
add_executable(test_cfg 
    tests/test_cfg.cpp
    src/ControlFlowGraph.cpp
    src/CapstoneDisassembler.cpp
)

# Capstone 库路径（项目内嵌版本）
set(CAPSTONE_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../src/vm/capstone-Release")
include_directories("${CAPSTONE_ROOT}/include")

# 直接使用完整的库文件路径
set(CAPSTONE_LIB "${CAPSTONE_ROOT}/lib/libcapstone.a")
target_link_libraries(test_cfg "${CAPSTONE_LIB}")
message(STATUS "Using embedded Capstone from: ${CAPSTONE_ROOT}")
message(STATUS "Capstone library: ${CAPSTONE_LIB}")

# 输出提示
message(STATUS "Phase 2.3 CFG project configured")
message(STATUS "Build with: cmake --build .")
message(STATUS "Run tests: ./test_cfg")
