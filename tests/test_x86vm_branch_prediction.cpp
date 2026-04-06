#include "../include/vm/X86CPU.h"
#include <iostream>
#include <cassert>
#include <vector>

void test_branch_prediction() {
    std::cout << "\n=== Test 3: Branch Prediction (Loop) ===" << std::endl;
    
    X86VMConfig config;
    config.memory_size = 64 * 1024;
    config.entry_point = 0x1000;
    config.stack_pointer = 0xFFFF;
    
    X86CPUVM vm(3, config);
    
    // 加载带循环的代码
    std::vector<uint8_t> test_code = {
        0xB9, 0x05, 0x00, 0x00, 0x00,  // mov ecx, 5  (循环计数器)
        0xB8, 0x00, 0x00, 0x00, 0x00,  // mov eax, 0  (累加器)
        
        // 循环开始 (0x100A)
        0x83, 0xC0, 0x01,              // add eax, 1
        0xFF, 0xC9,                    // dec ecx
        0x75, 0xF9,                    // jne 0x100A  (向后跳转，偏移量=-7)
        
        0xC3                           // ret
    };
    
    vm.load_binary(test_code, config.entry_point);
    vm.build_cfg(config.entry_point);
    
    vm.start();
    
    int steps = 0;
    while (vm.get_x86_state() == X86VMState::RUNNING && steps < 100) {
        vm.execute_instruction();
        steps++;
    }
    
    // 验证：eax 应该等于 5
    uint64_t eax = vm.get_register(X86Reg::RAX);
    std::cout << "[INFO] EAX = " << eax << std::endl;
    
    if (eax == 5) {
        std::cout << "[PASS] Loop executed correctly, EAX = " << eax << std::endl;
    } else {
        std::cout << "[FAIL] Loop execution failed, EAX = " << eax << " (expected 5)" << std::endl;
    }
    
    std::cout << "[INFO] Total steps: " << steps << std::endl;
    
    vm.stop();
    
    std::cout << "\n[Test 3 Complete]" << std::endl;
}

int main() {
    std::ios::sync_with_stdio(false);
    std::cin.tie(nullptr);std::cout.tie(nullptr); // 保证输入输出同步

    try {
        test_branch_prediction();
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "[ERROR] Exception: " << e.what() << std::endl;
        return 1;
    }
}
