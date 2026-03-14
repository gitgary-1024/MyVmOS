#include "vm/mVM.h"
#include "vm/VmManager.h"
#include "router/RouterCore.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <vector>

int main() {
    system("chcp 65001 > nul");

    std::cout << "===== VM调度系统测试 =====" << std::endl;
    
    // 启动 VM Manager（包含调度器）
    VmManager::instance().start();
    
    // 创建多个 VM 实例
    std::vector<std::shared_ptr<mVM>> vms;
    
    // VM0: 执行简单加法
    std::vector<uint32_t> program0 = {
        0x01000000,  // NOP
        0x01010000,  // ADD R0, R0 (R0 += R0, 初始为 0)
        0x01000001,  // ADD R0, R1 (R0 += R1, R1=0)
        0x04000000   // HALT
    };
    
    auto vm0 = std::make_shared<mVM>(0);
    vm0->load_program(program0);
    vms.push_back(vm0);
    
    // VM1: 执行更多指令
    std::vector<uint32_t> program1 = {
        0x01000000,  // NOP
        0x01000000,  // NOP
        0x01000000,  // NOP
        0x01010000,  // ADD R0, R0
        0x04000000   // HALT
    };
    
    auto vm1 = std::make_shared<mVM>(1);
    vm1->load_program(program1);
    vms.push_back(vm1);
    
    // VM2: 执行最长程序
    std::vector<uint32_t> program2 = {
        0x01000000,  // NOP
        0x01000000,  // NOP
        0x01000000,  // NOP
        0x01000000,  // NOP
        0x01000000,  // NOP
        0x01010000,  // ADD R0, R0
        0x01010000,  // ADD R0, R0
        0x04000000   // HALT
    };
    
    auto vm2 = std::make_shared<mVM>(2);
    vm2->load_program(program2);
    vms.push_back(vm2);
    
    std::cout << "\n创建了 " << vms.size() << " 个 VM" << std::endl;
    std::cout << "启动所有 VM..." << std::endl;
    
    // 启动所有 VM
    for (auto& vm : vms) {
        vm->start();
    }
    
    // 等待一段时间，让调度器工作
    std::cout << "\n等待 500ms，观察调度行为..." << std::endl;
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    // 检查 VM 状态
    std::cout << "\n===== VM 状态检查 =====" << std::endl;
    for (size_t i = 0; i < vms.size(); i++) {
        auto& ctx = vms[i]->get_sched_ctx();
        std::cout << "VM" << i << ": "
                  << "状态=" << static_cast<int>(vms[i]->get_state()) 
                  << ", 优先级=" << ctx.priority
                  << ", 有名额=" << (ctx.has_quota ? "是" : "否")
                  << ", vruntime=" << ctx.vruntime
                  << ", PC=" << vms[i]->get_pc()
                  << std::endl;
    }
    
    // 停止所有 VM
    std::cout << "\n停止所有 VM..." << std::endl;
    for (auto& vm : vms) {
        vm->stop();
    }
    
    // 停止 VM Manager
    VmManager::instance().stop();
    
    std::cout << "\n===== 测试完成 =====" << std::endl;
    return 0;
}
