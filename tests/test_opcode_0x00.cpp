#include <iostream>
#include "vm/X86CPU.h"

int main() {
    system("chcp 65001 > nul");
    std::cout << "=== Testing X86VM Opcode 0x00 Fix ===" << std::endl;
    
    // 创建 X86VM
    X86CPUVM vm(1);
    
    // 编写测试程序：ADD r/m8, r8 (0x00)
    // 测试：AL = 5, BL = 3, ADD AL, BL → AL = 8
    std::vector<uint32_t> program = {
        0xB0, 0x05,        // MOV AL, 5
        0xB3, 0x03,        // MOV BL, 3
        0x00, 0xD8,        // ADD AL, BL  (opcode 0x00, ModRM: reg=BL(011), rm=AL(000))
        0xF4               // HLT
    };
    
    vm.load_program(program);
    vm.start();
    
    // 等待执行完成
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // 检查 RAX 的低 8 位（AL）是否为 8
    uint64_t rax = vm.get_register(X86Reg::RAX);
    uint8_t al = static_cast<uint8_t>(rax & 0xFF);
    
    std::cout << "RAX = " << rax << std::endl;
    std::cout << "AL (low 8 bits) = " << static_cast<int>(al) << std::endl;
    std::cout << "Expected: 8 (5 + 3)" << std::endl;
    
    if (al == 8) {
        std::cout << "✅ Test PASSED: Opcode 0x00 works correctly!" << std::endl;
        return 0;
    } else {
        std::cout << "❌ Test FAILED: Expected AL=8, got " << static_cast<int>(al) << std::endl;
        return 1;
    }
}
