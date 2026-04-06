#include <iostream>
#include <thread>
#include "vm/X86CPU.h"

int main() {
    system("chcp 65001 > nul");
    std::cout << "=== Testing X86VM with REX Prefix Fix ===" << std::endl;
    
    // 创建 X86VM
    X86CPUVM vm(1);
    
    // 编写测试程序：简单的 32 位 MOV 和 ADD
    // 指令序列（字节）：
    // B8 05 00 00 00       - MOV EAX, 5
    // BB 03 00 00 00       - MOV EBX, 3
    // 01 D8                - ADD EAX, EBX
    // F4                   - HLT
    
    // 转换为 uint32_t 数组（小端序，每 4 个字节一组）：
    std::vector<uint32_t> program = {
        0x000000B8,  // MOV EAX, imm32 (操作码)
        0x00000005,  // 立即数 5
        0x000000BB,  // MOV EBX, imm32 (操作码)
        0x00000003,  // 立即数 3
        0x00D80100,  // ADD EAX, EBX (01 D8) + 填充
        0x0000F400   // HLT (F4) + 填充
    };
    
    vm.load_program(program);
    
    std::cout << "Program loaded at 0x" << std::hex << vm.get_rip() << std::dec << std::endl;
    std::cout << "Initial RAX = " << vm.get_register(X86Reg::RAX) << std::endl;
    std::cout << "Initial RBX = " << vm.get_register(X86Reg::RBX) << std::endl;
    
    // 在后台线程运行 VM
    std::thread vm_thread([&vm]() {
        vm.run_loop();
    });
    
    // 等待执行完成（或者检测到 HLT）
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    
    // 检查状态
    std::cout << "Final state: " << static_cast<int>(vm.get_state()) << std::endl;
    
    uint64_t rax = vm.get_register(X86Reg::RAX);
    uint64_t rbx = vm.get_register(X86Reg::RBX);
    
    std::cout << "Final RAX = " << rax << " (expected: 8)" << std::endl;
    std::cout << "Final RBX = " << rbx << " (expected: 3)" << std::endl;
    
    if (rax == 8 && rbx == 3) {
        std::cout << "ok Test PASSED!" << std::endl;
        vm_thread.join();
        return 0;
    } else {
        std::cout << "REEOR Test FAILED" << std::endl;
        vm_thread.join();
        return 1;
    }
}
