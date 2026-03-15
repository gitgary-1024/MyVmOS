#include <iostream>
#include <thread>
#include <chrono>
#include "../include/vm/mVM.h"
#include "../include/vm/VmManager.h"
#include "../include/vm/SchedulerManager.h"
#include "../include/vm/Scheduler/CFS_Scheduler.h"
#include "../include/vm/Scheduler/EDF_Scheduler.h"
#include "../include/vm/Scheduler/Hybrid_Scheduler.h"
#include "../include/router/RouterCore.h"
#include "../include/log/Logging.h"

using namespace std;

/**
 * @brief 测试所有调度器的基本功能（不启动独立线程）
 */
void test_all_schedulers() {
    cout << "=== 测试所有调度器（基本功能）===" << endl << endl;
    
    // 初始化路由器和 VM 管理器
    RouterCore& router = RouterCore::instance();
    VmManager& vm_mgr = VmManager::instance();
    
    // 创建 3 个 VM
    vector<std::shared_ptr<mVM>> vms;
    for (int i = 0; i < 3; ++i) {
        auto vm = make_shared<mVM>(i);
        
        // 加载简单程序
        std::vector<uint32_t> program = {
            0x00,  // NOP
            0x01,  // ADD R0, R0
            0x00,  // NOP
            0x04   // HALT
        };
        vm->load_program(program);
        
        vm_mgr.register_vm(vm);
        vms.push_back(vm);
    }
    
    cout << endl;
    
    // 测试 1: CFS 调度器
    cout << "--- 测试 1: CFS 调度器 ---" << endl;
    auto cfs_sched = make_shared<vm::CFSScheduler>();
    
    // 注册 VM 到 CFS 调度器
    for (auto& vm : vms) {
        cfs_sched->register_vm(vm->get_vm_id(), vm);
    }
    cout << "[CFS] 已注册 " << vms.size() << " 个 VM" << endl;
    
    // 测试 get_scheduler_name
    cout << "CFS 调度器名称：" << cfs_sched->get_scheduler_name() << endl;
    cout << "CFS 调度器类型：" << typeid(*cfs_sched).name() << endl;
    cout << endl;
    
    // 测试 2: EDF 调度器
    cout << "--- 测试 2: EDF 调度器 ---" << endl;
    auto edf_sched = make_shared<vm::EDFScheduler>();
    
    // 注册 VM 到 EDF 调度器
    for (auto& vm : vms) {
        edf_sched->register_vm(vm->get_vm_id(), vm);
    }
    cout << "[EDF] 已注册 " << vms.size() << " 个 VM" << endl;
    
    // 测试 get_scheduler_name
    cout << "EDF 调度器名称：" << edf_sched->get_scheduler_name() << endl;
    cout << "EDF 调度器类型：" << typeid(*edf_sched).name() << endl;
    cout << endl;
    
    // 测试 3: Hybrid 调度器
    cout << "--- 测试 3: Hybrid(CFS+EDF) 调度器 ---" << endl;
    auto hybrid_sched = make_shared<vm::HybridScheduler>();
    
    // 注册 VM 到 Hybrid 调度器
    for (auto& vm : vms) {
        hybrid_sched->register_vm(vm->get_vm_id(), vm);
    }
    cout << "[Hybrid] 已注册 " << vms.size() << " 个 VM" << endl;
    
    // 测试 get_scheduler_name
    cout << "Hybrid 调度器名称：" << hybrid_sched->get_scheduler_name() << endl;
    cout << "Hybrid 调度器类型：" << typeid(*hybrid_sched).name() << endl;
    cout << endl;
    
    cout << "=== 测试完成 ===" << endl;
}

int main() {
    system("chcp 65001 > nul");
    std::cout << "正在测试调度器基本功能..." << std::endl;
    test_all_schedulers();
    std::cout << "测试完成" << std::endl;
    return 0;
}
