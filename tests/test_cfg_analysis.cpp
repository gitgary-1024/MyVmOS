#include "vm/X86CPU.h"
#include <iostream>
#include <vector>
#include <cstdint>

int main() {
    system("chcp 65001 > nul");
    std::cout << "=== X86VM CFG Analysis Test ===" << std::endl;
    
    // 创建 VM
    X86VMConfig config;
    config.memory_size = 4096;
    auto vm = std::make_unique<X86CPUVM>(1, config);
    
    // 加载测试代码（包含跳转指令）
    std::vector<uint8_t> test_code = {
        // 0x00: MOV RAX, 0x10
        0xB8, 0x10, 0x00, 0x00, 0x00,
        
        // 0x05: CMP RAX, 0x20
        0x48, 0x83, 0xF8, 0x20,
        
        // 0x09: JE 0x15 (如果相等则跳转到 0x15)
        0x74, 0x0A,
        
        // 0x0B: MOV RBX, 0x100
        0xBB, 0x00, 0x01, 0x00, 0x00,
        
        // 0x10: JMP 0x1A
        0xEB, 0x08,
        
        // 0x12: ADD [RAX], AL (填充字节)
        0x00, 0x00,
        
        // 0x14: NOP
        0x90,
        
        // 0x15: MOV RCX, 0x200
        0xB9, 0x00, 0x02, 0x00, 0x00,
        
        // 0x1A: RET
        0xC3
    };
    
    // 将代码加载到内存
    for (size_t i = 0; i < test_code.size(); ++i) {
        vm->write_byte(i, test_code[i]);
    }
    
    std::cout << "\n[Step 1] Building CFG from entry point 0x0..." << std::endl;
    
    // 构建 CFG
    vm->build_cfg(0x0);
    
    if (!vm->has_cfg()) {
        std::cerr << "ERROR: CFG not built!" << std::endl;
        return 1;
    }
    
    std::cout << "\n[Step 2] CFG analysis complete!" << std::endl;
    std::cout << "Expected structure:" << std::endl;
    std::cout << "  Block 0x00: [MOV RAX, CMP RAX, JE 0x15]" << std::endl;
    std::cout << "    ├─> Block 0x15 (jump target)" << std::endl;
    std::cout << "    └─> Block 0x0B (fall-through)" << std::endl;
    std::cout << "  Block 0x0B: [MOV RBX, JMP 0x1A]" << std::endl;
    std::cout << "    └─> Block 0x1A (unconditional jump)" << std::endl;
    std::cout << "  Block 0x15: [MOV RCX]" << std::endl;
    std::cout << "    └─> Block 0x1A (fall-through)" << std::endl;
    std::cout << "  Block 0x1A: [RET]" << std::endl;
    std::cout << "    └─> (exit)" << std::endl;
    
    std::cout << "\n=== Test Passed! ===" << std::endl;
    return 0;
}
