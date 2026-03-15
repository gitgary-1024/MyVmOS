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
 * @brief 测试所有调度器（CFS、EDF、Hybrid）
 */
void test_all_schedulers() {
    cout << "=== 测试所有调度器 ===" << endl << endl;
    
    // 初始化路由器和 VM管理器
    RouterCore& router = RouterCore::instance();
    VmManager& vm_mgr = VmManager::instance();
    SchedulerManager& sched_mgr = SchedulerManager::instance();
    
    // 创建 3 个 VM
    vector<std::shared_ptr<mVM>> vms;
    for (int i = 0; i < 3; ++i) {
        auto vm = make_shared<mVM>(i);  // ✅ 传入 vm_id
        
        // 加载简单程序（NOP + ADD）
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
    
    // 测试 1: 默认调度器（PBDS）
    cout << "--- 测试 1: 默认调度器 (PBDS) ---" << endl;
    cout << "当前调度器：" << sched_mgr.get_current_scheduler_name() << endl;
    this_thread::sleep_for(chrono::milliseconds(50));
    
    // 执行几个 VM
    for (int i = 0; i < 3; ++i) {
        if (vms[i]->has_quota()) {
            vms[i]->execute_instruction();
        }
    }
    
    cout << "VM0: 状态=" << (int)vms[0]->get_state() 
         << ", vruntime=" << vms[0]->get_sched_ctx().vruntime
         << ", PC=" << vms[0]->get_pc() << endl;
    cout << "VM1: 状态=" << (int)vms[1]->get_state() 
         << ", vruntime=" << vms[1]->get_sched_ctx().vruntime
         << ", PC=" << vms[1]->get_pc() << endl;
    cout << "VM2: 状态=" << (int)vms[2]->get_state() 
         << ", vruntime=" << vms[2]->get_sched_ctx().vruntime
         << ", PC=" << vms[2]->get_pc() << endl;
    cout << endl;
    
    // 测试 2: 切换到 CFS 调度器
    cout << "--- 测试 2: CFS 调度器 ---" << endl;
    auto cfs_sched = make_shared<vm::CFSScheduler>();
    
    // 先将 VM 注册到 CFS 调度器
    for (auto& vm : vms) {
        cfs_sched->register_vm(vm->get_vm_id(), vm);
    }
    cout << "[CFS] 已注册 " << vms.size() << " 个 VM" << endl;
    
    sched_mgr.set_scheduler(cfs_sched);
    cout << "当前调度器：" << sched_mgr.get_current_scheduler_name() << endl;
    
    // 立即停止调度器，避免卡住
    if (cfs_sched) {
        cfs_sched->stop();
    }
    cout << "[CFS] 已停止调度器线程" << endl;
    
    // 执行 VM
    for (int i = 0; i < 3; ++i) {
        if (vms[i]->has_quota()) {
            vms[i]->execute_instruction();
        }
    }
    
    cout << "VM0: vruntime=" << vms[0]->get_sched_ctx().vruntime 
         << ", PC=" << vms[0]->get_pc() << endl;
    cout << "VM1: vruntime=" << vms[1]->get_sched_ctx().vruntime 
         << ", PC=" << vms[1]->get_pc() << endl;
    cout << "VM2: vruntime=" << vms[2]->get_sched_ctx().vruntime 
         << ", PC=" << vms[2]->get_pc() << endl;
    cout << endl;
    
    // 测试 3: 切换到 EDF 调度器
    cout << "--- 测试 3: EDF 调度器 ---" << endl;
    auto edf_sched = make_shared<vm::EDFScheduler>();
    
    // 先将 VM 注册到 EDF 调度器
    for (auto& vm : vms) {
        edf_sched->register_vm(vm->get_vm_id(), vm);
    }
    cout << "[EDF] 已注册 " << vms.size() << " 个 VM" << endl;
    
    sched_mgr.set_scheduler(edf_sched);
    cout << "当前调度器：" << sched_mgr.get_current_scheduler_name() << endl;
    
    // 立即停止调度器
    if (edf_sched) {
        edf_sched->stop();
    }
    cout << "[EDF] 已停止调度器线程" << endl;
    
    // 执行 VM
    for (int i = 0; i < 3; ++i) {
        if (vms[i]->has_quota()) {
            vms[i]->execute_instruction();
        }
    }
    
    cout << "VM0: vruntime=" << vms[0]->get_sched_ctx().vruntime 
         << ", PC=" << vms[0]->get_pc() << endl;
    cout << "VM1: vruntime=" << vms[1]->get_sched_ctx().vruntime 
         << ", PC=" << vms[1]->get_pc() << endl;
    cout << "VM2: vruntime=" << vms[2]->get_sched_ctx().vruntime 
         << ", PC=" << vms[2]->get_pc() << endl;
    cout << endl;
    
    // 测试 4: 切换到混合调度器
    cout << "--- 测试 4: Hybrid(CFS+EDF) 调度器 ---" << endl;
    auto hybrid_sched = make_shared<vm::HybridScheduler>();
    
    // 先将 VM 注册到混合调度器
    for (auto& vm : vms) {
        hybrid_sched->register_vm(vm->get_vm_id(), vm);
    }
    cout << "[Hybrid] 已注册 " << vms.size() << " 个 VM" << endl;
    
    sched_mgr.set_scheduler(hybrid_sched);
    cout << "当前调度器：" << sched_mgr.get_current_scheduler_name() << endl;
    
    // 立即停止调度器
    if (hybrid_sched) {
        hybrid_sched->stop();
    }
    cout << "[Hybrid] 已停止调度器线程" << endl;
    
    // 执行 VM
    for (int i = 0; i < 3; ++i) {
        if (vms[i]->has_quota()) {
            vms[i]->execute_instruction();
        }
    }
    
    cout << "VM0: vruntime=" << vms[0]->get_sched_ctx().vruntime 
         << ", PC=" << vms[0]->get_pc() << endl;
    cout << "VM1: vruntime=" << vms[1]->get_sched_ctx().vruntime 
         << ", PC=" << vms[1]->get_pc() << endl;
    cout << "VM2: vruntime=" << vms[2]->get_sched_ctx().vruntime 
         << ", PC=" << vms[2]->get_pc() << endl;
    cout << endl;
    
    cout << "=== 测试完成 ===" << endl;
}

int main() {
    system("chcp 65001 > nul");
    std::cout << "正在测试所有调度器..." << std::endl;
    test_all_schedulers();
    std::cout << "测试完成" << std::endl;
    return 0;
}
