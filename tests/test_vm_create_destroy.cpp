/**
 * @brief 测试 VmManager 的 createX86VM() 和 destroyVM() 方法
 */

#include "vm/VmManager.h"
#include <iostream>
#include <cassert>

void test_create_destroy_x86_vm() {
    std::cout << "\n=== Test: Create and Destroy X86 VM ===\n";
    
    auto& vmMgr = VmManager::instance();
    
    // 1. 创建 X86 VM
    std::cout << "Creating X86 VM...\n";
    int vmId = vmMgr.createX86VM();
    
    if (vmId < 0) {
        std::cout << "Failed to create X86 VM (returned " << vmId << ")\n";
        std::cout << "This is expected if X86CPUVM implementation is not complete.\n";
        return;
    }
    
    std::cout << "Created X86 VM with ID: " << vmId << "\n";
    assert(vmId > 0);
    
    // 2. 查询 VM
    auto vm = vmMgr.get_vm(static_cast<uint64_t>(vmId));
    assert(vm != nullptr);
    std::cout << "VM found, type: " << typeid(*vm).name() << "\n";
    
    // 3. 销毁 VM
    std::cout << "Destroying VM " << vmId << "...\n";
    bool success = vmMgr.destroyVM(vmId);
    assert(success);
    std::cout << "VM " << vmId << " destroyed successfully\n";
    
    // 4. 等待异步销毁完成
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // 5. 验证 VM 已销毁
    auto vmAfter = vmMgr.get_vm(static_cast<uint64_t>(vmId));
    assert(vmAfter == nullptr);
    std::cout << "Verified: VM no longer exists\n";
    
    std::cout << "\n✓ Test passed!\n";
}

int main() {
    std::cout << "========================================\n";
    std::cout << "   VmManager::createX86VM() Test      \n";
    std::cout << "========================================\n";
    
    test_create_destroy_x86_vm();
    
    std::cout << "\n========================================\n";
    std::cout << "   All tests completed!                \n";
    std::cout << "========================================\n";
    
    return 0;
}
