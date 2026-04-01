/**
 * @brief RuntimeInterface 使用示例
 * 
 * 展示如何使用运行时交互接口进行热交互：
 * 1. 创建 VM 并查询状态
 * 2. 修改寄存器值
 * 3. 外设控制
 * 4. 发送测试消息
 * 5. 系统监控
 * 6. 命令行交互
 */

#include "../utils/RuntimeInterface.h"
#include <iostream>
#include <thread>
#include <chrono>

void example1_basic_usage() {
    std::cout << "\n=== Example 1: Basic Usage ===\n";
    
    // 1. 获取单例实例
    auto& runtime = RuntimeInterface::getInstance();
    
    // 2. 初始化（传入 VmManager 和 PeriphManager）
    // VmManager* vmMgr = ...;
    // PeriphManager* periphMgr = ...;
    // runtime.initialize(vmMgr, periphMgr);
    
    // 3. 创建 VM
    int vmId = runtime.createVM("X86");
    std::cout << "Created VM with ID: " << vmId << "\n";
    
    // 4. 查询 VM 状态
    auto status = runtime.getVMStatus(vmId);
    std::cout << "VM State: " << status.state << "\n";
    
    // 5. 读取/修改寄存器
    uint32_t eax = runtime.readRegister(vmId, "EAX");
    std::cout << "EAX: " << eax << "\n";
    
    runtime.writeRegister(vmId, "EAX", 42);
    std::cout << "EAX set to 42\n";
    
    // 6. 控制系统状态
    runtime.startVM(vmId);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    runtime.pauseVM(vmId);
    
    // 7. 销毁 VM
    runtime.destroyVM(vmId);
}

void example2_command_line_interface() {
    std::cout << "\n=== Example 2: Command Line Interface ===\n";
    
    auto& runtime = RuntimeInterface::getInstance();
    
    // 执行命令并获取输出
    std::vector<std::string> commands = {
        "help",
        "vm list",
        "vm create X86",
        "vm list",
        "system status",
        "periph list",
        "vm destroy 0",
        "vm list"
    };
    
    for (const auto& cmd : commands) {
        std::cout << "\n>>> " << cmd << "\n";
        std::string output = runtime.executeCommand(cmd);
        std::cout << output;
    }
}

void example3_status_callback() {
    std::cout << "\n=== Example 3: Status Callbacks ===\n";
    
    auto& runtime = RuntimeInterface::getInstance();
    
    // 注册状态变化回调
    runtime.registerStatusCallback([](const std::string& event, int vmId, const std::string& details) {
        std::cout << "[EVENT] " << event;
        if (vmId >= 0) {
            std::cout << " (VM " << vmId << ")";
        }
        std::cout << ": " << details << "\n";
    });
    
    // 触发事件
    int vmId = runtime.createVM("X86");
    runtime.startVM(vmId);
    runtime.writeRegister(vmId, "EBX", 100);
    runtime.stopVM(vmId);
    runtime.destroyVM(vmId);
}

void example4_memory_operations() {
    std::cout << "\n=== Example 4: Memory Operations ===\n";
    
    auto& runtime = RuntimeInterface::getInstance();
    
    int vmId = runtime.createVM("X86");
    
    // 写入内存
    uint8_t test_data[] = {0x90, 0x90, 0xC3};  // NOP, NOP, RET
    runtime.writeMemory(vmId, test_data, 0x1000, sizeof(test_data));
    
    // 读取内存
    uint8_t buffer[256];
    runtime.readMemory(vmId, buffer, 0x1000, sizeof(test_data));
    
    std::cout << "Memory at 0x1000: ";
    for (size_t i = 0; i < sizeof(test_data); ++i) {
        printf("%02X ", buffer[i]);
    }
    std::cout << "\n";
    
    runtime.destroyVM(vmId);
}

void example5_interactive_console() {
    std::cout << "\n=== Example 5: Interactive Console ===\n";
    std::cout << "Starting interactive console (type 'quit' to exit)...\n\n";
    
    auto& runtime = RuntimeInterface::getInstance();
    
    std::string line;
    while (true) {
        std::cout << "runtime> ";
        std::getline(std::cin, line);
        
        if (line == "quit" || line == "exit") {
            break;
        }
        
        std::string output = runtime.executeCommand(line);
        std::cout << output;
    }
}

int main() {
    std::cout << "========================================\n";
    std::cout << "   Runtime Interface Usage Examples    \n";
    std::cout << "========================================\n";
    
    // 示例 1：基本用法
    example1_basic_usage();
    
    // 示例 2：命令行接口
    example2_command_line_interface();
    
    // 示例 3：状态回调
    example3_status_callback();
    
    // 示例 4：内存操作
    example4_memory_operations();
    
    // 示例 5：交互式控制台（需要用户输入）
    // example5_interactive_console();
    
    std::cout << "\n========================================\n";
    std::cout << "   All examples completed!             \n";
    std::cout << "========================================\n";
    
    return 0;
}
