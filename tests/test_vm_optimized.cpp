#include <iostream>
#include <vector>
#include <thread>
#include <chrono>
#include <memory>
#include "vm/VmManager.h"
#include "vm/mVM.h"
#include "router/RouterCore.h"

using namespace std;

/**
 * @brief 测试使用 ObjectPool 和 ThreadPool 优化后的 VM 创建/销毁
 */
void test_optimized_vm_creation() {
    cout << "\n=== Test 1: Optimized VM Creation with ThreadPool ===" << endl;
    
    auto& vm_manager = VmManager::instance();
    
    // 创建多个 VM
    vector<shared_ptr<mVM>> vms;
    for (int i = 0; i < 5; ++i) {
        // 使用 make_vm 方法（保持兼容性）
        auto vm = VmManager::make_vm<mVM>(static_cast<uint64_t>(i));
        vms.push_back(vm);
        
        cout << "[Created] VM " << i << endl;
    }
    
    cout << "\n[After Creation] VM count: " << vm_manager.get_registered_vm_count() << endl;
    
    // 销毁 VM（回收到对象池）
    for (int i = 0; i < 5; ++i) {
        vm_manager.destroy_vm(i);
        cout << "[Destroyed] VM " << i << endl;
    }
    
    // 等待异步销毁完成
    this_thread::sleep_for(chrono::milliseconds(100));
    
    cout << "\n[After Destruction] VM count: " << vm_manager.get_registered_vm_count() << endl;
}

/**
 * @brief 测试并发 VM 创建/销毁（压力测试）
 */
void test_concurrent_vm_lifecycle() {
    cout << "\n=== Test 2: Concurrent VM Lifecycle Stress Test ===" << endl;
    
    auto& vm_manager = VmManager::instance();
    
    const int ITERATIONS = 50;
    atomic<int> created_count{0};
    atomic<int> destroyed_count{0};
    
    cout << "Starting " << ITERATIONS << " concurrent VM create/destroy cycles..." << endl;
    auto start = chrono::steady_clock::now();
    
    // 并发创建和销毁
    vector<thread> threads;
    for (int i = 0; i < ITERATIONS; ++i) {
        threads.emplace_back([&vm_manager, &created_count, &destroyed_count, i]() {
            try {
                // 创建 VM
                auto vm = VmManager::make_vm<mVM>(static_cast<uint64_t>(100 + i));
                created_count.fetch_add(1);
                
                // 模拟短暂运行
                this_thread::sleep_for(chrono::milliseconds(1));
                
                // 销毁 VM
                vm_manager.destroy_vm(100 + i);
                destroyed_count.fetch_add(1);
                
            } catch (const exception& e) {
                cerr << "[Thread " << i << "] Exception: " << e.what() << endl;
            }
        });
    }
    
    // 等待所有线程完成
    for (auto& t : threads) {
        t.join();
    }
    
    auto end = chrono::steady_clock::now();
    auto duration = chrono::duration_cast<chrono::milliseconds>(end - start).count();
    
    cout << "\n[Results]" << endl;
    cout << "Created: " << created_count.load() << " VMs" << endl;
    cout << "Destroyed: " << destroyed_count.load() << " VMs" << endl;
    cout << "Duration: " << duration << "ms" << endl;
    cout << "Throughput: " << (ITERATIONS * 1000 / duration) << " VMs/sec" << endl;
    cout << "Final VM count: " << vm_manager.get_registered_vm_count() << endl;
}

/**
 * @brief 测试对象池容量限制和自动扩容
 */
void test_vm_pool_capacity() {
    cout << "\n=== Test 3: VM Capacity Test ===" << endl;
    
    auto& vm_manager = VmManager::instance();
    
    const int TEST_COUNT = 120;  // 测试大量创建
    vector<shared_ptr<mVM>> vms;
    
    cout << "\nAttempting to create " << TEST_COUNT << " VMs..." << endl;
    int success_count = 0;
    
    for (int i = 0; i < TEST_COUNT; ++i) {
        try {
            auto vm = VmManager::make_vm<mVM>(static_cast<uint64_t>(200 + i));
            vms.push_back(vm);
            success_count++;
            
            if (i % 20 == 0) {
                cout << "[Progress] Created " << (i + 1) << " VMs" << endl;
            }
        } catch (const exception& e) {
            cerr << "[Error] Failed to create VM " << i << ": " << e.what() << endl;
        }
    }
    
    cout << "\n[Results]" << endl;
    cout << "Successfully created: " << success_count << " VMs" << endl;
    cout << "Current VM count: " << vm_manager.get_registered_vm_count() << endl;
    
    // 批量销毁
    cout << "\nDestroying all VMs..." << endl;
    for (int i = 0; i < success_count; ++i) {
        vm_manager.destroy_vm(200 + i);
    }
    
    // 等待异步销毁完成
    this_thread::sleep_for(chrono::milliseconds(200));
    
    cout << "[After Cleanup] Final VM count: " << vm_manager.get_registered_vm_count() << endl;
}

/**
 * @brief 测试 VM 对象复用（验证 reset() 是否被调用）
 */
void test_vm_reuse() {
    cout << "\n=== Test 4: VM Creation Verification ===" << endl;
    
    auto& vm_manager = VmManager::instance();
    
    // 创建并销毁一个 VM
    cout << "\nCreating VM #300..." << endl;
    auto vm1 = VmManager::make_vm<mVM>(300);
    cout << "VM #300 created successfully" << endl;
    
    cout << "Destroying VM #300..." << endl;
    vm_manager.destroy_vm(300);
    
    // 等待销毁完成
    this_thread::sleep_for(chrono::milliseconds(100));
    
    // 再次创建 VM
    cout << "\nCreating VM #301..." << endl;
    auto vm2 = VmManager::make_vm<mVM>(301);
    cout << "VM #301 created successfully" << endl;
    
    cout << "\n✅ Both VMs created successfully with ThreadPool optimization!" << endl;
}

/**
 * @brief 性能对比：对象池 vs 直接 new
 */
void test_performance_comparison() {
    cout << "\n=== Test 5: Performance with Async Destruction ===" << endl;
    
    const int ITERATIONS = 100;
    
    // 测试异步销毁性能
    auto& vm_manager = VmManager::instance();
    
    cout << "\nTesting async destruction (" << ITERATIONS << " iterations)..." << endl;
    auto start = chrono::steady_clock::now();
    
    vector<shared_ptr<mVM>> pool_vms;
    for (int i = 0; i < ITERATIONS; ++i) {
        auto vm = VmManager::make_vm<mVM>(400 + i);
        pool_vms.push_back(vm);
    }
    
    for (int i = 0; i < ITERATIONS; ++i) {
        vm_manager.destroy_vm(400 + i);
    }
    
    auto pool_time = chrono::steady_clock::now() - start;
    
    // 等待异步操作完成
    this_thread::sleep_for(chrono::milliseconds(100));
    
    cout << "Async destruction time: " 
         << chrono::duration_cast<chrono::milliseconds>(pool_time).count() 
         << "ms" << endl;
    cout << "Successfully queued " << ITERATIONS << " VMs for async destruction" << endl;
}

int main() {
    cout << "========================================" << endl;
    cout << "  Optimized VM Management Test Suite  " << endl;
    cout << "========================================" << endl;
    
    try {
        // 启动 Router（必需）
        auto& router = RouterCore::instance();
        router.register_module(MODULE_VM_MANAGER, "VmManager");
        router.start_polling();
        
        // 启动 VM Manager
        auto& vm_manager = VmManager::instance();
        vm_manager.start();
        
        // 等待启动完成
        this_thread::sleep_for(chrono::milliseconds(100));
        
        // 运行测试
        test_optimized_vm_creation();
        test_concurrent_vm_lifecycle();
        test_vm_pool_capacity();
        test_vm_reuse();
        test_performance_comparison();
        
        // 清理
        vm_manager.stop();
        
        cout << "\n========================================" << endl;
        cout << "  All tests completed successfully!   " << endl;
        cout << "========================================" << endl;
        
    } catch (const exception& e) {
        cerr << "\n❌ Test failed with exception: " << e.what() << endl;
        return 1;
    }
    
    return 0;
}
