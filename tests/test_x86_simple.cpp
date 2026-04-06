#include "../include/vm/X86CPU.h"
#include <iostream>
#include <vector>

using namespace std;

int main() {
    system("chcp 65001 > nul");
    cout << "=== X86 VM 单步调试 ===" << endl;
    
    // 创建 VM
    X86VMConfig config;
    config.memory_size = 1024 * 1024;  // 1MB
    config.entry_point = 0x1000;
    config.stack_pointer = 0x8000;
    
    X86CPUVM vm(1, config);
    
    // 最简单的测试：MOV RAX, 42 (48 B8 2A 00 00 00 00 00 00 00)
    vector<uint8_t> program = {
        0x48, 0xB8, 0x2A, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // MOV RAX, 42
        0xF4  // HLT
    };
    
    vm.load_binary(program, 0x1000);  // 加载到地址 0x1000
    
    cout << "初始状态:" << endl;
    vm.dump_registers();
    
    cout << "\n开始执行..." << endl;
    vm.start();
    
    // 手动执行指令直到 HLT
    // int max_cycles = 100;
    // while (max_cycles-- > 0) {
    //     vm.execute_instruction();
    //     if (vm.get_state() == X86VMState::HALTED || vm.get_state() == X86VMState::TERMINATED)
    //         break;
    // }
    
    cout << "\n最终状态:" << endl;
    vm.dump_registers();
    
    uint64_t rax = vm.get_register(X86Reg::RAX);
    cout << "\nRAX = " << rax << " (期望：42)" << endl;
    
    if (rax == 42) {
        cout << "✓ 测试通过!" << endl;
        return 0;
    } else {
        cout << "✗ 测试失败!" << endl;
        return 1;
    }
}
