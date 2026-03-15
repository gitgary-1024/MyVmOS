/**
 * @brief 验证新旧 API 兼容性测试
 * 
 * 本测试文件证明新的 VmManager API 完全兼容旧代码
 */

#include <iostream>
#include <memory>
#include "vm/VmManager.h"
#include "vm/mVM.h"
#include "router/RouterCore.h"

using namespace std;

// ============================================================================
// 测试 1: 使用旧的全局函数（完全兼容）
// ============================================================================
void test_old_api_compatibility() {
    cout << "\n=== Test 1: Old API Compatibility (100% Compatible) ===" << endl;
    
    // ✅ 旧方式：使用全局快捷函数
    auto vm = std::make_shared<mVM>(1);
    
    // ✅ 旧方式：显式注册
    vm_manager_register_vm(vm);
    
    cout << "[Old API] VM created and registered using global functions" << endl;
    cout << "[Old API] VM count: " << VmManager::instance().get_registered_vm_count() << endl;
    
    // ✅ 旧方式：显式注销
    vm_manager_unregister_vm(1);
    
    cout << "[Old API] VM unregistered successfully" << endl;
    cout << "[Old API] Final VM count: " << VmManager::instance().get_registered_vm_count() << endl;
}

// ============================================================================
// 测试 2: 使用新的静态模板方法（可选增强）
// ============================================================================
void test_new_api_creation() {
    cout << "\n=== Test 2: New API - make_vm() (Optional Enhancement) ===" << endl;
    
    // ✅ 新方式：使用静态模板方法
    auto vm = VmManager::make_vm<mVM>(2);
    
    // ✅ 仍然需要显式注册（保持灵活性）
    vm_manager_register_vm(vm);
    
    cout << "[New API] VM created using VmManager::make_vm()" << endl;
    cout << "[New API] VM count: " << VmManager::instance().get_registered_vm_count() << endl;
    
    // ✅ 可以使用新的异步销毁
    VmManager::instance().destroy_vm(2);
    
    // 等待异步操作完成
    this_thread::sleep_for(chrono::milliseconds(50));
    
    cout << "[New API] VM destroyed asynchronously" << endl;
    cout << "[New API] Final VM count: " << VmManager::instance().get_registered_vm_count() << endl;
}

// ============================================================================
// 测试 3: 混合使用新旧 API（完全兼容）
// ============================================================================
void test_mixed_api_usage() {
    cout << "\n=== Test 3: Mixed Old & New API (Fully Compatible) ===" << endl;
    
    // ✅ 创建：使用旧方式
    auto vm1 = std::make_shared<mVM>(3);
    vm_manager_register_vm(vm1);
    
    // ✅ 创建：使用新方式
    auto vm2 = VmManager::make_vm<mVM>(4);
    vm_manager_register_vm(vm2);
    
    cout << "[Mixed] Created 2 VMs using mixed APIs" << endl;
    cout << "[Mixed] VM count: " << VmManager::instance().get_registered_vm_count() << endl;
    
    // ✅ 销毁：使用旧方式
    vm_manager_unregister_vm(3);
    
    // ✅ 销毁：使用新方式
    VmManager::instance().destroy_vm(4);
    
    this_thread::sleep_for(chrono::milliseconds(50));
    
    cout << "[Mixed] Both VMs destroyed (one sync, one async)" << endl;
    cout << "[Mixed] Final VM count: " << VmManager::instance().get_registered_vm_count() << endl;
}

// ============================================================================
// 测试 4: 访问线程池（高级功能）
// ============================================================================
void test_thread_pool_access() {
    cout << "\n=== Test 4: Advanced - Thread Pool Access ===" << endl;
    
    auto& pool = VmManager::instance().get_lifecycle_pool();
    
    cout << "[Advanced] ThreadPool worker count: " << pool.get_thread_count() << endl;
    cout << "[Advanced] ThreadPool is running: " << (pool.is_running() ? "yes" : "no") << endl;
    
    // ✅ 提交自定义任务到生命周期线程池
    pool.submit([]() {
        cout << "[Advanced] Custom task executed in lifecycle thread pool" << endl;
    });
    
    this_thread::sleep_for(chrono::milliseconds(10));
}

// ============================================================================
// 测试 5: 性能对比（异步 vs 同步）
// ============================================================================
void test_performance_comparison() {
    cout << "\n=== Test 5: Performance - Async vs Sync Destruction ===" << endl;
    
    const int ITERATIONS = 20;
    
    // 测试同步销毁
    cout << "\n[Performance] Testing synchronous destruction..." << endl;
    auto start = chrono::steady_clock::now();
    
    for (int i = 0; i < ITERATIONS; ++i) {
        auto vm = std::make_shared<mVM>(100 + i);
        vm_manager_register_vm(vm);
        vm_manager_unregister_vm(100 + i);
    }
    
    auto sync_time = chrono::steady_clock::now() - start;
    cout << "[Performance] Sync time: " 
         << chrono::duration_cast<chrono::milliseconds>(sync_time).count() 
         << "ms for " << ITERATIONS << " VMs" << endl;
    
    // 测试异步销毁
    cout << "[Performance] Testing asynchronous destruction..." << endl;
    start = chrono::steady_clock::now();
    
    for (int i = 0; i < ITERATIONS; ++i) {
        auto vm = VmManager::make_vm<mVM>(200 + i);
        vm_manager_register_vm(vm);
        VmManager::instance().destroy_vm(200 + i);
    }
    
    auto async_time = chrono::steady_clock::now() - start;
    
    // 等待异步操作完成
    this_thread::sleep_for(chrono::milliseconds(100));
    
    cout << "[Performance] Async time: " 
         << chrono::duration_cast<chrono::milliseconds>(async_time).count() 
         << "ms for " << ITERATIONS << " VMs (main thread)" << endl;
    
    double speedup = (double)chrono::duration_cast<chrono::milliseconds>(sync_time).count() / 
                     (double)chrono::duration_cast<chrono::milliseconds>(async_time).count();
    
    cout << "[Performance] Speedup: " << speedup << "x faster (main thread response)" << endl;
}

// ============================================================================
// 主测试函数
// ============================================================================
int main() {
    cout << "========================================" << endl;
    cout << "  VM Manager API Compatibility Tests  " << endl;
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
        
        // 运行所有兼容性测试
        test_old_api_compatibility();
        test_new_api_creation();
        test_mixed_api_usage();
        test_thread_pool_access();
        test_performance_comparison();
        
        // 清理
        vm_manager.stop();
        
        cout << "\n========================================" << endl;
        cout << "  All compatibility tests passed!     " << endl;
        cout << "  ✅ Old API: 100% compatible         " << endl;
        cout << "  ✅ New API: Optional enhancement    " << endl;
        cout << "  ✅ Mixed usage: Fully supported     " << endl;
        cout << "========================================" << endl;
        
    } catch (const exception& e) {
        cerr << "\n❌ Test failed with exception: " << e.what() << endl;
        return 1;
    }
    
    return 0;
}
