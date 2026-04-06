#include "../include/vm/X86CPU.h"
#include <iostream>
#include <cassert>
#include <vector>

void test_full_integration() {
    std::cout << "\n=== Test 4: Full Integration (Complex Control Flow) ===" << std::endl;
    
    X86VMConfig config;
    config.memory_size = 64 * 1024;
    config.entry_point = 0x1000;
    config.stack_pointer = 0xFFFF;
    
    X86CPUVM vm(4, config);
    
    // 复杂测试代码：条件分支 + CALL + RET
    std::vector<uint8_t> test_code = {
        // 主函数 (0x1000)
        0xB8, 0x05, 0x00, 0x00, 0x00,  // mov eax, 5
        0x83, 0xF8, 0x03,              // cmp eax, 3
        0x7E, 0x07,                    // jle 0x100E  (如果 eax <= 3)
        
        // 分支 1: eax > 3
        0xE8, 0x0A, 0x00, 0x00, 0x00,  // call 0x101A
        0xEB, 0x05,                    // jmp 0x101F
        
        // 分支 2: eax <= 3 (0x100E)
        0xB8, 0x00, 0x00, 0x00, 0x00,  // mov eax, 0
        0xEB, 0x03,                    // jmp 0x101F
        
        // 返回点 (0x1014)
        0x90,                          // nop
        
        // 子函数 (0x101A)
        0xB8, 0x0A, 0x00, 0x00, 0x00,  // mov eax, 10
        0xC3,                          // ret
        
        // 结束 (0x101F)
        0xC3                           // ret
    };
    
    vm.load_binary(test_code, config.entry_point);
    
    // 构建 CFG
    vm.build_cfg(config.entry_point);
    
    // 执行
    vm.start();
    
    int steps = 0;
    while (vm.get_x86_state() == X86VMState::RUNNING && steps < 50) {
        vm.execute_instruction();
        steps++;
    }
    
    // 验证：eax 应该等于 10（因为 5 > 3，调用子函数）
    uint64_t eax = vm.get_register(X86Reg::RAX);
    std::cout << "[INFO] EAX = " << eax << std::endl;
    
    if (eax == 10) {
        std::cout << "[PASS] Complex control flow executed correctly" << std::endl;
        std::cout << "[PASS] EAX = " << eax << " (expected 10)" << std::endl;
    } else {
        std::cout << "[FAIL] Complex control flow failed, EAX = " << eax << " (expected 10)" << std::endl;
    }
    
    std::cout << "[INFO] Total steps: " << steps << std::endl;
    
    vm.stop();
    
    std::cout << "\n[Test 4 Complete]" << std::endl;
}

int main() {
    try {
        test_full_integration();
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "[ERROR] Exception: " << e.what() << std::endl;
        return 1;
    }
}
