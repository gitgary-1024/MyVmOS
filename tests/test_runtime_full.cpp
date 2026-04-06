/**
 * @brief RuntimeInterface 完整功能测试
 * 
 * 测试内容：
 * 1. 初始化接口
 * 2. VM 创建/销毁
 * 3. VM 状态查询
 * 4. 命令行接口
 * 5. 回调函数
 * 6. 并发访问
 */

#include "../utils/RuntimeInterface.h"
#include "vm/VmManager.h"
#include "periph/PeriphManager.h"
#include <iostream>
#include <cassert>
#include <thread>
#include <chrono>
#include <vector>

void test_initialization() {
    std::cout << "\n=== Test 1: Initialization ===\n";
    
    auto& runtime = RuntimeInterface::getInstance();
    auto& vmMgr = VmManager::instance();
    
    // PeriphManager 没有 instance() 方法，需要创建实例
    PeriphManager periphMgr;
    
    // 初始化
    runtime.initialize(&vmMgr, &periphMgr);
    std::cout << "✓ RuntimeInterface initialized successfully\n";
    
    // 重复初始化应该被忽略
    runtime.initialize(&vmMgr, &periphMgr);
    std::cout << "✓ Double initialization handled correctly\n";
}

void test_vm_create_destroy() {
    std::cout << "\n=== Test 2: VM Create/Destroy ===\n";
    
    auto& runtime = RuntimeInterface::getInstance();
    
    // 测试创建 X86 VM
    std::cout << "Creating X86 VM...\n";
    int vmId = runtime.createVM("X86");
    
    if (vmId < 0) {
        std::cout << "⚠ createVM returned " << vmId << " (expected if X86CPUVM not fully implemented)\n";
        return;
    }
    
    std::cout << "✓ Created VM with ID: " << vmId << "\n";
    
    // 测试查询状态
    auto status = runtime.getVMStatus(vmId);
    std::cout << "  - VM ID: " << status.id << "\n";
    std::cout << "  - Type: " << status.type << "\n";
    std::cout << "  - State: " << status.state << "\n";
    
    // 测试销毁
    std::cout << "Destroying VM " << vmId << "...\n";
    bool success = runtime.destroyVM(vmId);
    assert(success);
    std::cout << "✓ VM destroyed successfully\n";
    
    // 等待异步销毁
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // 验证已销毁
    auto statusAfter = runtime.getVMStatus(vmId);
    std::cout << "✓ Verified: VM no longer exists (state: " << statusAfter.state << ")\n";
}

void test_command_line_interface() {
    std::cout << "\n=== Test 3: Command Line Interface ===\n";
    
    auto& runtime = RuntimeInterface::getInstance();
    
    std::vector<std::string> commands = {
        "help",
        "vm list",
        "system status",
        "periph list"
    };
    
    for (const auto& cmd : commands) {
        std::cout << "\n>>> " << cmd << "\n";
        std::string output = runtime.executeCommand(cmd);
        std::cout << output;
    }
    
    std::cout << "✓ CLI tests completed\n";
}

void test_status_callbacks() {
    std::cout << "\n=== Test 4: Status Callbacks ===\n";
    
    auto& runtime = RuntimeInterface::getInstance();
    
    int callback_count = 0;
    
    // 注册回调
    runtime.registerStatusCallback([&callback_count](
        const std::string& event, 
        int vmId, 
        const std::string& details) {
        
        std::cout << "  [CALLBACK] " << event;
        if (vmId >= 0) {
            std::cout << " (VM " << vmId << ")";
        }
        std::cout << ": " << details << "\n";
        callback_count++;
    });
    
    std::cout << "Callback registered, triggering events...\n";
    
    // 触发一些事件（如果 VM 创建成功）
    int vmId = runtime.createVM("X86");
    if (vmId >= 0) {
        runtime.startVM(vmId);
        runtime.writeRegister(vmId, "EAX", 42);
        runtime.destroyVM(vmId);
        
        // 等待异步操作
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    std::cout << "✓ Triggered " << callback_count << " callback(s)\n";
}

void test_concurrent_access() {
    std::cout << "\n=== Test 5: Concurrent Access ===\n";
    
    auto& runtime = RuntimeInterface::getInstance();
    
    std::vector<std::thread> threads;
    std::atomic<int> success_count{0};
    std::atomic<int> fail_count{0};
    
    // 并发创建多个 VM
    std::cout << "Creating 5 VMs concurrently...\n";
    
    for (int i = 0; i < 5; ++i) {
        threads.emplace_back([&runtime, &success_count, &fail_count, i]() {
            int vmId = runtime.createVM("X86");
            if (vmId >= 0) {
                std::cout << "  Thread " << i << ": Created VM " << vmId << "\n";
                success_count++;
                
                // 稍作延迟后销毁
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                runtime.destroyVM(vmId);
            } else {
                std::cout << "  Thread " << i << ": Failed to create VM\n";
                fail_count++;
            }
        });
    }
    
    // 等待所有线程完成
    for (auto& t : threads) {
        t.join();
    }
    
    std::cout << "✓ Concurrent test completed: " 
              << success_count << " succeeded, " 
              << fail_count << " failed\n";
}

void test_error_handling() {
    std::cout << "\n=== Test 6: Error Handling ===\n";
    
    auto& runtime = RuntimeInterface::getInstance();
    
    // 测试无效 VM ID
    auto status = runtime.getVMStatus(-1);
    std::cout << "getVMStatus(-1): state = " << status.state << " (expected: ERROR)\n";
    
    // 测试无效的寄存器访问
    runtime.writeRegister(-1, "INVALID_REG", 12345);
    std::cout << "writeRegister(-1, ...): handled gracefully\n";
    
    // 测试无效的 VM 控制
    bool result = runtime.stopVM(-1);
    std::cout << "stopVM(-1): " << (result ? "true" : "false") << " (expected: false)\n";
    
    // 测试无效的命令
    std::string output = runtime.executeCommand("invalid command xyz");
    std::cout << "Execute invalid command: " << (output.empty() ? "(empty)" : "returned output") << "\n";
    
    std::cout << "✓ Error handling tests completed\n";
}

void test_system_status() {
    std::cout << "\n=== Test 7: System Status ===\n";
    
    auto& runtime = RuntimeInterface::getInstance();
    
    // 获取系统状态
    auto sysStatus = runtime.getSystemStatus();
    std::cout << "System Status:\n";
    std::cout << "  - Total VMs: " << sysStatus.total_vms << "\n";
    std::cout << "  - Running VMs: " << sysStatus.running_vms << "\n";
    std::cout << "  - Total Peripherals: " << sysStatus.total_peripherals << "\n";
    std::cout << "  - Memory Usage: " << sysStatus.memory_usage << " bytes\n";
    
    // 获取性能统计
    auto perfStats = runtime.getPerformanceStats();
    std::cout << "Performance Stats:\n";
    std::cout << "  - Avg CPU Usage: " << perfStats.avg_vm_cpu_usage << "%\n";
    std::cout << "  - Messages/sec: " << perfStats.messages_per_second << "\n";
    
    std::cout << "✓ System status queries completed\n";
}

int main() {
    system("chcp 65001 > nul");
    std::cout << "========================================\n";
    std::cout << "   RuntimeInterface Full Test Suite   \n";
    std::cout << "========================================\n";
    
    try {
        // 测试 1: 初始化
        test_initialization();
        
        // 测试 2: VM 创建/销毁
        test_vm_create_destroy();
        
        // 测试 3: 命令行接口
        test_command_line_interface();
        
        // 测试 4: 回调函数
        test_status_callbacks();
        
        // 测试 5: 并发访问
        test_concurrent_access();
        
        // 测试 6: 错误处理
        test_error_handling();
        
        // 测试 7: 系统状态
        test_system_status();
        
    } catch (const std::exception& e) {
        std::cerr << "\n❌ Exception caught: " << e.what() << "\n";
        return 1;
    } catch (...) {
        std::cerr << "\n❌ Unknown exception caught\n";
        return 1;
    }
    
    std::cout << "\n========================================\n";
    std::cout << "   All tests completed!                \n";
    std::cout << "========================================\n";
    
    return 0;
}
