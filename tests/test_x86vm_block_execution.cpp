#include "../include/vm/X86CPU.h"
#include <iostream>
#include <cassert>
#include <vector>

void test_block_execution() {
    std::cout << "\n=== Test 2: Basic Block Execution ===" << std::endl;
    
    X86VMConfig config;
    config.memory_size = 64 * 1024;
    config.entry_point = 0x1000;
    config.stack_pointer = 0xFFFF;
    
    X86CPUVM vm(2, config);
    
    // 加载线性代码（无跳转）
    std::vector<uint8_t> test_code = {
        0xB8, 0x0A, 0x00, 0x00, 0x00,  // mov eax, 10
        0xBB, 0x14, 0x00, 0x00, 0x00,  // mov ebx, 20
        0x01, 0xD8,                    // add eax, ebx  (eax = 30)
        0xC3                           // ret
    };
    
    vm.load_binary(test_code, config.entry_point);
    vm.build_cfg(config.entry_point);
    
    // 执行
    vm.start();
    
    int steps = 0;
    while (vm.get_x86_state() == X86VMState::RUNNING && steps < 10) {
        vm.execute_instruction();
        steps++;
    }
    
    // 验证结果
    uint64_t eax = vm.get_register(X86Reg::RAX);
    std::cout << "[INFO] EAX = " << eax << std::endl;
    
    if (eax == 30) {
        std::cout << "[PASS] EAX = " << eax << " (expected 30)" << std::endl;
    } else {
        std::cout << "[FAIL] EAX = " << eax << " (expected 30)" << std::endl;
    }
    
    std::cout << "[INFO] Executed " << steps << " instruction(s)/block(s)" << std::endl;
    
    vm.stop();
    
    std::cout << "\n[Test 2 Complete]" << std::endl;
}

int main() {
    try {
        test_block_execution();
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "[ERROR] Exception: " << e.what() << std::endl;
        return 1;
    }
}
