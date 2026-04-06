#include "vm/X86CPU.h"
#include <iostream>
#include <vector>
#include <cassert>

void test_modrm_basic() {
    std::cout << "=== Testing Basic ModR/M Instructions ===" << std::endl;
    
    X86VMConfig config;
    config.memory_size = 1024 * 1024;  // 1MB
    config.entry_point = 0x1000;
    
    X86CPUVM vm(1, config);
    
    // 测试 MOV r64, r/m64 (0x8B) - 寄存器到寄存器
    std::vector<uint8_t> code1 = {
        0x48, 0xB8, 0x78, 0x56, 0x34, 0x12, 0x00, 0x00, 0x00, 0x00,  // MOV RAX, 0x12345678
        0x48, 0x89, 0xC3,  // MOV RBX, RAX
        0xF4  // HLT
    };
    
    vm.load_binary(code1, config.entry_point);
    vm.set_rip(config.entry_point);
    vm.start();  // 启动 VM
    
    std::cout << "Test 1: MOV RBX, RAX" << std::endl;
    while (vm.get_x86_state() == X86VMState::RUNNING) {
        vm.execute_instruction();
    }
    
    uint64_t rbx = vm.get_register(X86Reg::RBX);
    std::cout << "RBX = 0x" << std::hex << rbx << std::dec << std::endl;
    assert(rbx == 0x12345678 && "MOV register to register failed");
    std::cout << "✓ Test 1 passed" << std::endl << std::endl;
}

void test_modrm_memory() {
    std::cout << "=== Testing ModR/M Memory Operations ===" << std::endl;
    
    X86VMConfig config;
    config.memory_size = 1024 * 1024;
    config.entry_point = 0x1000;
    
    X86CPUVM vm(2, config);
    
    // 测试 MOV r64, [rax] (0x8B) - 内存到寄存器
    std::vector<uint8_t> code = {
        0x48, 0xB8, 0x00, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // MOV RAX, 0x2000
        0x48, 0xC7, 0x00, 0xAB, 0xCD, 0xEF, 0x12, 0x00, 0x00, 0x00, 0x00,  // MOV [RAX], 0x12EFCDAB
        0x48, 0x8B, 0x18,  // MOV RBX, [RAX]
        0xF4  // HLT
    };
    
    vm.load_binary(code, config.entry_point);
    vm.set_rip(config.entry_point);
    vm.start();  // 启动 VM
    
    std::cout << "Test 2: MOV RBX, [RAX]" << std::endl;
    while (vm.get_x86_state() == X86VMState::RUNNING) {
        vm.execute_instruction();
    }
    
    uint64_t rbx = vm.get_register(X86Reg::RBX);
    std::cout << "RBX = 0x" << std::hex << rbx << std::dec << std::endl;
    assert(rbx == 0x12EFCDAB && "MOV memory to register failed");
    std::cout << "✓ Test 2 passed" << std::endl << std::endl;
}

void test_modrm_with_displacement() {
    std::cout << "=== Testing ModR/M with Displacement ===" << std::endl;
    
    X86VMConfig config;
    config.memory_size = 1024 * 1024;
    config.entry_point = 0x1000;
    
    X86CPUVM vm(3, config);
    
    // 测试 MOV r64, [rax + disp8] (0x8B) - 带位移的内存访问
    std::vector<uint8_t> code = {
        0x48, 0xB8, 0x00, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // MOV RAX, 0x2000
        0x48, 0xC7, 0x40, 0x10, 0x12, 0x34, 0x56, 0x78,  // MOV QWORD [RAX + 0x10], 0x78563412 (32-bit imm)
        0x48, 0x8B, 0x58, 0x10,  // MOV RBX, [RAX + 0x10]
        0xF4  // HLT
    };
    
    vm.load_binary(code, config.entry_point);
    vm.set_rip(config.entry_point);
    vm.start();  // 启动 VM
    
    std::cout << "Test 3: MOV RBX, [RAX + 0x10]" << std::endl;
    while (vm.get_x86_state() == X86VMState::RUNNING) {
        vm.execute_instruction();
    }
    
    uint64_t rbx = vm.get_register(X86Reg::RBX);
    std::cout << "RBX = 0x" << std::hex << rbx << std::dec << std::endl;
    assert(rbx == 0x78563412 && "MOV with displacement failed");
    std::cout << "✓ Test 3 passed" << std::endl << std::endl;
}

void test_arithmetic_modrm() {
    std::cout << "=== Testing Arithmetic with ModR/M ===" << std::endl;
    
    X86VMConfig config;
    config.memory_size = 1024 * 1024;
    config.entry_point = 0x1000;
    
    X86CPUVM vm(4, config);
    
    // 测试 ADD r64, r/m64 (0x03)
    std::vector<uint8_t> code = {
        0x48, 0xB8, 0x0A, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // MOV RAX, 10
        0x48, 0xB9, 0x14, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // MOV RCX, 20
        0x48, 0x03, 0xC1,  // ADD RAX, RCX
        0xF4  // HLT
    };
    
    vm.load_binary(code, config.entry_point);
    vm.set_rip(config.entry_point);
    vm.start();  // 启动 VM
    
    std::cout << "Test 4: ADD RAX, RCX" << std::endl;
    while (vm.get_x86_state() == X86VMState::RUNNING) {
        vm.execute_instruction();
    }
    
    uint64_t rax = vm.get_register(X86Reg::RAX);
    std::cout << "RAX = " << std::dec << rax << std::endl;
    assert(rax == 30 && "ADD with ModR/M failed");
    std::cout << "✓ Test 4 passed" << std::endl << std::endl;
}

int main() {
    std::cout << "Starting ModR/M Instruction Tests..." << std::endl << std::endl;
    
    try {
        test_modrm_basic();
        test_modrm_memory();
        test_modrm_with_displacement();
        test_arithmetic_modrm();
        
        std::cout << "========================================" << std::endl;
        std::cout << "All ModR/M tests passed successfully! ✓" << std::endl;
        std::cout << "========================================" << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Test failed with exception: " << e.what() << std::endl;
        return 1;
    }
}
