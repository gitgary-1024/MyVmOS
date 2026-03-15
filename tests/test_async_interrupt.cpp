/**
 * @brief 异步中断处理&中断优先级测试
 * 
 * 测试内容：
 * 1. 中断优先级队列
 * 2. 异步中断处理
 * 3. 中断超时处理
 * 4. 中断统计信息
 */

#include <iostream>
#include <memory>
#include <thread>
#include <chrono>
#include "vm/VmManager.h"
#include "vm/mVM.h"
#include "router/RouterCore.h"

using namespace std;

// ============================================================================
// 测试 1: 中断优先级定义
// ============================================================================
void test_interrupt_priority() {
    cout << "\n=== Test 1: Interrupt Priority Definitions ===" << endl;
    
    // 测试不同中断类型的默认优先级
    cout << "KEYBOARD priority: " << static_cast<int>(get_interrupt_priority(InterruptType::KEYBOARD)) 
         << " (expected HIGH=2)" << endl;
    cout << "MOUSE priority: " << static_cast<int>(get_interrupt_priority(InterruptType::MOUSE)) 
         << " (expected HIGH=2)" << endl;
    cout << "TERMINAL_INPUT priority: " << static_cast<int>(get_interrupt_priority(InterruptType::TERMINAL_INPUT)) 
         << " (expected NORMAL=1)" << endl;
    cout << "DISK_READ priority: " << static_cast<int>(get_interrupt_priority(InterruptType::DISK_READ)) 
         << " (expected LOW=0)" << endl;
    cout << "SYSTEM priority: " << static_cast<int>(get_interrupt_priority(InterruptType::SYSTEM)) 
         << " (expected REALTIME=3)" << endl;
}

// ============================================================================
// 测试 2: 中断请求结构体
// ============================================================================
void test_interrupt_request_struct() {
    cout << "\n=== Test 2: InterruptRequest Structure ===" << endl;
    
    // 测试默认构造
    InterruptRequest req1;
    cout << "Default request - vm_id=" << req1.vm_id 
         << ", periph_id=" << req1.periph_id
         << ", priority=" << static_cast<int>(req1.priority) << endl;
    
    // 测试设置中断类型（自动设置优先级）
    InterruptRequest req2;
    req2.vm_id = 1;
    req2.periph_id = 5;
    req2.set_interrupt_type(InterruptType::KEYBOARD);
    req2.timeout_ms = 3000;
    
    cout << "Keyboard request - vm_id=" << req2.vm_id 
         << ", periph_id=" << req2.periph_id
         << ", type=" << static_cast<int>(req2.interrupt_type)
         << ", priority=" << static_cast<int>(req2.priority)
         << ", timeout=" << req2.timeout_ms << "ms" << endl;
    
    // 测试带时间戳
    req2.enqueue_time = std::chrono::steady_clock::now();
    cout << "Request has enqueue_time: yes" << endl;
}

// ============================================================================
// 测试 3: 中断结果结构体
// ============================================================================
void test_interrupt_result_struct() {
    cout << "\n=== Test 3: InterruptResult Structure ===" << endl;
    
    auto start_time = std::chrono::steady_clock::now();
    
    InterruptResult result;
    result.vm_id = 1;
    result.return_value = 42;
    result.is_timeout = false;
    result.interrupt_type = InterruptType::DISK_READ;
    result.priority = InterruptPriority::LOW;
    result.completion_time = std::chrono::steady_clock::now();
    
    int64_t latency = result.get_total_latency_ms(start_time);
    cout << "Interrupt result - vm_id=" << result.vm_id 
         << ", return_value=" << result.return_value
         << ", type=" << static_cast<int>(result.interrupt_type)
         << ", priority=" << static_cast<int>(result.priority)
         << ", latency=" << latency << "ms" << endl;
}

// ============================================================================
// 测试 4: 中断优先级队列
// ============================================================================
void test_interrupt_priority_queue() {
    cout << "\n=== Test 4: Interrupt Priority Queue (Simulation) ===" << endl;
    
    // 模拟优先级队列行为
    struct TestInterrupt {
        uint64_t vm_id;
        InterruptPriority priority;
        std::chrono::steady_clock::time_point enqueue_time;
        
        bool operator<(const TestInterrupt& other) const {
            if (static_cast<int>(priority) != static_cast<int>(other.priority)) {
                return static_cast<int>(priority) < static_cast<int>(other.priority);
            }
            return enqueue_time > other.enqueue_time;
        }
    };
    
    std::priority_queue<TestInterrupt> pq;
    
    // 创建不同优先级的中断
    TestInterrupt low_prio{1, InterruptPriority::LOW, std::chrono::steady_clock::now()};
    TestInterrupt high_prio{2, InterruptPriority::HIGH, std::chrono::steady_clock::now()};
    TestInterrupt normal_prio{3, InterruptPriority::NORMAL, std::chrono::steady_clock::now()};
    TestInterrupt realtime_prio{4, InterruptPriority::REALTIME, std::chrono::steady_clock::now()};
    
    // 入队（顺序打乱）
    pq.push(low_prio);
    pq.push(realtime_prio);
    pq.push(normal_prio);
    pq.push(high_prio);
    
    // 出队（应该按优先级降序）
    cout << "Dequeue order (should be: REALTIME=3, HIGH=2, NORMAL=1, LOW=0):" << endl;
    while (!pq.empty()) {
        const auto& interrupt = pq.top();
        cout << "  VM " << interrupt.vm_id 
             << ", priority=" << static_cast<int>(interrupt.priority) << endl;
        pq.pop();
    }
}

// ============================================================================
// 测试 5: 完整的异步中断流程
// ============================================================================
void test_async_interrupt_flow() {
    cout << "\n=== Test 5: Complete Async Interrupt Flow ===" << endl;
    
    // 初始化路由器和 VM Manager
    RouterCore& router = RouterCore::instance();
    VmManager& vm_manager = VmManager::instance();
    
    // 创建测试 VM
    auto vm = VmManager::make_vm<mVM>(1);
    
    // 加载简单程序
    std::vector<uint32_t> program = {
        0x00,  // NOP
        0x04   // HALT
    };
    vm->load_program(program);
    
    // 注册 VM
    vm_manager.register_vm(vm);
    cout << "VM 1 registered" << endl;
    
    // 发送中断请求
    cout << "Sending interrupt request..." << endl;
    vm->send_interrupt_request(1, 2000);  // 外设 1，超时 2 秒
    
    // 等待一小段时间让工作线程处理
    this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // 获取统计信息
    VmManager::InterruptStats stats = vm_manager.get_interrupt_stats();
    cout << "Interrupt stats:" << endl;
    cout << "  Total requests: " << stats.total_requests << endl;
    cout << "  Pending requests: " << stats.pending_requests << endl;
    cout << "  Completed requests: " << stats.completed_requests << endl;
    cout << "  Timeout requests: " << stats.timeout_requests << endl;
    cout << "  Average latency: " << stats.avg_latency_ms << "ms" << endl;
    
    // 清理
    vm_manager.unregister_vm(1);
    cout << "VM 1 unregistered" << endl;
}

// ============================================================================
// 测试 6: 中断超时处理
// ============================================================================
void test_interrupt_timeout() {
    cout << "\n=== Test 6: Interrupt Timeout Handling ===" << endl;
    
    RouterCore& router = RouterCore::instance();
    VmManager& vm_manager = VmManager::instance();
    
    // 创建测试 VM
    auto vm = VmManager::make_vm<mVM>(2);
    std::vector<uint32_t> program = {0x04};  // HALT
    vm->load_program(program);
    vm_manager.register_vm(vm);
    
    cout << "VM 2 registered" << endl;
    
    // 发送一个很短的中断请求（100ms 超时）
    cout << "Sending short-timeout interrupt request (100ms)..." << endl;
    vm->send_interrupt_request(99, 100);  // 不存在的设备，会超时
    
    // 等待超时 + 一些余量
    cout << "Waiting for timeout..." << endl;
    this_thread::sleep_for(std::chrono::milliseconds(300));
    
    // 获取统计信息
    VmManager::InterruptStats stats = vm_manager.get_interrupt_stats();
    cout << "After timeout - Interrupt stats:" << endl;
    cout << "  Total requests: " << stats.total_requests << endl;
    cout << "  Timeout requests: " << stats.timeout_requests << endl;
    
    // 清理
    vm_manager.unregister_vm(2);
    cout << "VM 2 unregistered" << endl;
}

// ============================================================================
// 主测试函数
// ============================================================================
int main() {
    cout << "========================================" << endl;
    cout << "  Async Interrupt & Priority Tests     " << endl;
    cout << "========================================" << endl;
    
    try {
        // 启动路由器
        auto& router = RouterCore::instance();
        router.register_module(MODULE_VM_MANAGER, "VmManager");
        router.start_polling();
        
        // 启动 VM Manager
        auto& vm_manager = VmManager::instance();
        vm_manager.start();
        
        // 等待启动完成
        this_thread::sleep_for(std::chrono::milliseconds(100));
        
        // 运行所有测试
        test_interrupt_priority();
        test_interrupt_request_struct();
        test_interrupt_result_struct();
        test_interrupt_priority_queue();
        test_async_interrupt_flow();
        test_interrupt_timeout();
        
        // 清理
        vm_manager.stop();
        
        cout << "\n========================================" << endl;
        cout << "  All tests completed successfully!    " << endl;
        cout << "========================================" << endl;
        
    } catch (const exception& e) {
        cerr << "\n❌ Test failed with exception: " << e.what() << endl;
        return 1;
    }
    
    return 0;
}
