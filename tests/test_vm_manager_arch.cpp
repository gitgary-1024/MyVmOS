#include <iostream>
#include <vector>
#include <thread>
#include <chrono>
#include <sstream>
#include "vm/mVM.h"
#include "vm/VmManager.h"
#include "router/RouterCore.h"
#include "periph/Terminal.h"
#include "periph/PeriphManager.h"
#include "log/Logging.h"

int main() {
    std::cout << "=== Testing VM Manager Interrupt Architecture ===" << std::endl;
    
    // 1. 启动 Router
    auto& router = RouterCore::instance();
    router.register_module(0x01, "VMManager");
    router.register_module(0x03, "InterruptScheduler");
    router.register_module(0x04, "PeriphManager");
    router.start_polling();
    
    // 2. 启动 VM Manager
    auto& vm_manager = VmManager::instance();
    vm_manager.start();
    
    // 3. 创建并注册外设管理器
    auto periph_manager = std::make_shared<PeriphManager>(1);  // 1 个外设（Terminal）
    router.register_module(0x04, "PeriphManager");
    
    // 等待 Router、VM Manager 和外设管理器启动
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // 4. 创建并注册 Terminal 外设（ID=3）
    // Terminal 内部会订阅 INTERRUPT_REQUEST 消息
    // 注意：这里不需要手动注册，Terminal 构造函数会自动注册到 ID=3
    auto terminal = std::make_unique<Terminal>(periph_manager.get());
    
    std::cout << "Terminal peripheral created and registered (ID=3)" << std::endl;
    
    // 3. 创建 VM
    auto vm = std::make_shared<mVM>(1);
    
    // 加载测试程序：INPUT -> ADD -> OUTPUT -> HALT
    // 指令格式：[opcode:8 位][r1:8 位][r2/imm:16 位]
    // INPUT R1: opcode=2, r1=1 → (2) | (1<<8) = 0x0102
    // ADD R1, R1: opcode=1, r1=1, r2=1 → (1) | (1<<8) | (1<<16) = 0x010101
    // OUTPUT R1: opcode=3, r1=1 → (3) | (1<<8) = 0x0103
    // HALT: opcode=4 → 0x04
    std::vector<uint32_t> program = {
        0x00000102,  // INPUT R1  (触发中断到 Terminal，结果存入 R1)
        0x00010101,  // ADD R1, R1  (R1 += R1)
        0x00000103,  // OUTPUT R1 (输出 R1 的值)
        0x00000004   // HALT
    };
    vm->load_program(program);
    
    // 启动 VM
    vm->start();
    
    // 等待 VM 执行到 INPUT 指令并挂起
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    std::cout << "\n=== VM is waiting for keyboard input ===" << std::endl;
    std::cout << "Terminal is now waiting for your keyboard input (GETCH is blocking)..." << std::endl;
    std::cout << "Please type a character and press Enter to continue test...\n" << std::endl;
    
    // 等待 VM 执行完成（包括 ADD、OUTPUT、HALT）
    std::cout << "\nWaiting for VM to complete execution..." << std::endl;
    std::this_thread::sleep_for(std::chrono::milliseconds(3000));
    
    // 检查 VM 状态和输出
    std::cout << "\n=== VM Execution Results ===" << std::endl;
    std::cout << "VM state: " << (vm->get_state() == VMState::RUNNING ? "RUNNING" : "WAITING") << std::endl;
    std::cout << "VM R1 register: " << vm->get_register(1) << std::endl;
    std::cout << "Expected R1 value: 194 (97 * 2)" << std::endl;
    
    // 停止 VM
    vm->stop();
    
    // 停止 VM Manager 和 Router
    vm_manager.stop();
    router.stop();
    
    std::cout << "\n=== Test Complete ===" << std::endl;
    std::cout << "Expected flow:" << std::endl;
    std::cout << "1. VM executes INPUT instruction" << std::endl;
    std::cout << "2. VM sends INTERRUPT_REQUEST to VM Manager (MODULE_VM_MANAGER=0x01)" << std::endl;
    std::cout << "3. VM Manager validates and forwards to Router" << std::endl;
    std::cout << "4. Router delivers to Terminal peripheral" << std::endl;
    std::cout << "5. Terminal processes and sends result to VM Manager" << std::endl;
    std::cout << "6. VM Manager calls vm->handle_interrupt(result)" << std::endl;
    std::cout << "7. VM resumes execution with result in R1" << std::endl;
    std::cout << "8. VM executes ADD R1, R1 (R1 = 97 + 97 = 194)" << std::endl;
    std::cout << "9. VM executes OUTPUT R1 (outputs 194)" << std::endl;
    std::cout << "10. VM executes HALT" << std::endl;
    
    return 0;
}
