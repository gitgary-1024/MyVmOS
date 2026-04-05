cmake_minimum_required(VERSION 3.10)
project(MyOS VERSION 1.0 LANGUAGES CXX)

# 设置 C++17 标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 设置输出目录
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/lib)

# 添加头文件搜索路径
include_directories(
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include/log
    ${CMAKE_SOURCE_DIR}/include/periph
    ${CMAKE_SOURCE_DIR}/include/router
    ${CMAKE_SOURCE_DIR}/include/vm
    ${CMAKE_SOURCE_DIR}/include/vm/Scheduler
    ${CMAKE_SOURCE_DIR}/include/utils
    ${CMAKE_SOURCE_DIR}/include/utils/Loader  # Loader 模块
)

# 收集所有源文件
set(SOURCES
    src/log/Logging.cpp
    src/periph/PeriphManager.cpp
    src/periph/Terminal.cpp
    src/periph/Disk.cpp
    src/periph/NIC.cpp
    src/periph/TerminalManager.cpp  # ✅ 新增：终端管理器
    src/router/RouterCore.cpp
    src/vm/baseVM.cpp
    src/vm/mVM.cpp
    src/vm/VmManager.cpp
    src/vm/SchedulerManager.cpp
    src/vm/Scheduler/SchedulerBase.cpp
    src/vm/Scheduler/CFS_Scheduler.cpp
    src/vm/Scheduler/EDF_Scheduler.cpp
    src/vm/Scheduler/Hybrid_Scheduler.cpp
    src/vm/Scheduler/QuotaScheduler.cpp
    src/utils/TimeoutManager.cpp
    src/utils/ThreadPool.cpp
    src/utils/RuntimeInterface.cpp  # 新增：运行时交互接口
    src/utils/Loader/Loader.cpp     # Loader 模块
    src/vm/X86CPU.cpp
    src/vm/X86_decode.cpp
    src/vm/X86_instructions.cpp
    src/vm/X86_other_instructions.cpp
    # Capstone 反汇编模块
    src/vm/disassembly/CapstoneDisassembler.cpp
    src/vm/disassembly/x86/X86Instruction.cpp
    src/vm/disassembly/x86/X86ExecutorRegistry.cpp  # 新增：执行器注册表
)

# 创建静态库
add_library(myos_lib STATIC ${SOURCES})

# 设置线程支持
find_package(Threads REQUIRED)
target_link_libraries(myos_lib Threads::Threads)

# Windows 系统需要链接 ws2_32 (Winsock)
if(WIN32)
    target_link_libraries(myos_lib ws2_32)
endif()

# ==================== Compiler 子模块 ====================
# 添加自定义编译器作为子项目
option(BUILD_COMPILER "Build the custom compiler" ON)

if(BUILD_COMPILER)
    add_subdirectory(Compiler)
endif()

# ==================== Capstone 反汇编引擎 ====================
# 设置 Capstone 路径
set(CAPSTONE_INCLUDE_DIR ${CMAKE_SOURCE_DIR}/src/vm/capstone-Release/include)
set(CAPSTONE_LIB_DIR ${CMAKE_SOURCE_DIR}/src/vm/capstone-Release/lib)

# 添加 Capstone 头文件路径
include_directories(${CAPSTONE_INCLUDE_DIR})

# Capstone 反汇编测试
add_executable(test_capstone_disasm tests/test_capstone_disasm.cpp)
target_link_libraries(test_capstone_disasm ${CAPSTONE_LIB_DIR}/libcapstone.a)

# 新反汇编系统测试（基于组合模式）
add_executable(test_new_disassembly tests/test_new_disassembly.cpp)
target_link_libraries(test_new_disassembly myos_lib ${CAPSTONE_LIB_DIR}/libcapstone.a)

# IR + Executor 测试
add_executable(test_ir_executor tests/test_ir_executor.cpp)
target_link_libraries(test_ir_executor myos_lib ${CAPSTONE_LIB_DIR}/libcapstone.a)

# 反汇编集成测试
add_executable(test_disassembly_integration tests/test_disassembly_integration.cpp)
target_link_libraries(test_disassembly_integration myos_lib ${CAPSTONE_LIB_DIR}/libcapstone.a)

# ==================== 测试程序 ====================

# TimeoutManager 测试
add_executable(test_timeout tests/test_timeout_manager.cpp)
target_link_libraries(test_timeout myos_lib Threads::Threads)

# x86 CPU VM 测试
add_executable(test_x86_cpu tests/test_x86_cpu.cpp)
target_link_libraries(test_x86_cpu myos_lib)

# x86 简单测试
add_executable(test_x86_simple tests/test_x86_simple.cpp)
target_link_libraries(test_x86_simple myos_lib)

# Opcode 0x00 修复验证
add_executable(test_opcode_0x00 tests/test_opcode_0x00.cpp)
target_link_libraries(test_opcode_0x00 myos_lib)

# VM 算术运算测试
add_executable(test_vm_arithmetic tests/test_vm_arithmetic.cpp)
target_link_libraries(test_vm_arithmetic myos_lib)

# VmManager 架构测试
add_executable(test_vm_manager_arch tests/test_vm_manager_arch.cpp)
target_link_libraries(test_vm_manager_arch myos_lib Threads::Threads)

# VmManager 调度器测试
add_executable(test_vm_scheduler tests/test_vm_scheduler.cpp)
target_link_libraries(test_vm_scheduler myos_lib Threads::Threads)

# 所有调度器测试
add_executable(test_vm_all_schedulers tests/test_vm_all_schedulers_fixed.cpp)
target_link_libraries(test_vm_all_schedulers myos_lib Threads::Threads)

# 外设测试
add_executable(test_periph tests/test_periph_simple.cpp)
target_link_libraries(test_periph myos_lib Threads::Threads)

# 路由器测试
add_executable(test_router tests/test_router.cpp)
target_link_libraries(test_router myos_lib Threads::Threads)

# 日志测试
add_executable(test_logging tests/test_logging.cpp)
target_link_libraries(test_logging myos_lib)

# RuntimeInterface 测试
add_executable(test_runtime_full tests/test_runtime_full.cpp)
target_link_libraries(test_runtime_full myos_lib Threads::Threads)

# Loader 测试
add_executable(test_loader tests/test_loader.cpp)
target_link_libraries(test_loader myos_lib)

# VmManager 创建/销毁测试
add_executable(test_vm_create_destroy tests/test_vm_create_destroy.cpp)
target_link_libraries(test_vm_create_destroy myos_lib Threads::Threads)

# SYSCALL 测试
add_executable(test_vm_syscall tests/test_vm_syscall.cpp)
target_link_libraries(test_vm_syscall myos_lib Threads::Threads)

# ✅ Syscall 增强功能与终端管理器测试
add_executable(test_syscall_enhancements tests/test_syscall_enhancements.cpp)
target_link_libraries(test_syscall_enhancements myos_lib Threads::Threads)

# Compiler 集成测试
if(BUILD_COMPILER)
    add_executable(test_compiler_integration tests/test_compiler_integration.cpp)
    target_link_libraries(test_compiler_integration compiler_lib)
endif()

# ==================== 主程序 ====================

# 如果有 main.cpp，创建主程序
if(EXISTS ${CMAKE_SOURCE_DIR}/main.cpp)
    add_executable(main main.cpp)
    target_link_libraries(main myos_lib Threads::Threads)
endif()

# ==================== 自定义目标 ====================

# 清理目标
add_custom_target(clean_all
    COMMAND ${CMAKE_COMMAND} -E remove_directory ${CMAKE_SOURCE_DIR}/bin
    COMMAND ${CMAKE_COMMAND} -E remove_directory ${CMAKE_SOURCE_DIR}/lib
    COMMAND ${CMAKE_COMMAND} -E remove_directory ${CMAKE_SOURCE_DIR}/build
    COMMENT "Cleaning all build outputs"
)

# 运行所有测试
add_custom_target(run_all_tests
    COMMAND ${CMAKE_COMMAND} -E echo "Running TimeoutManager tests..."
    COMMAND ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/test_timeout
    COMMAND ${CMAKE_COMMAND} -E echo "Running VmManager tests..."
    COMMAND ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/test_vm_manager_arch
    COMMAND ${CMAKE_COMMAND} -E echo "Running Scheduler tests..."
    COMMAND ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/test_vm_scheduler
    COMMAND ${CMAKE_COMMAND} -E echo "All tests completed!"
    DEPENDS test_timeout test_vm_manager_arch test_vm_scheduler
    COMMENT "Running all tests"
)
