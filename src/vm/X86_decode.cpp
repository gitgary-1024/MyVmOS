#include "X86CPU.h"
#include <iostream>
#include <cstring>

// ===== ModR/M 解码 =====
X86CPUVM::ModRMDecoding X86CPUVM::decode_modrm(uint64_t rip, int& instruction_length) {
    ModRMDecoding decoding = {};
    
    uint8_t modrm_byte = read_byte(rip);
    decoding.mod = (modrm_byte >> 6) & 0x3;
    decoding.reg = (modrm_byte >> 3) & 0x7;
    decoding.rm = modrm_byte & 0x7;
    
    instruction_length = 1;  // ModR/M 字节本身
    
    // SIB 字节（简化处理，暂不支持）
    decoding.has_sib = false;
    
    // 位移计算
    if (decoding.mod == 0 && decoding.rm == 5) {
        // [disp32]
        decoding.has_displacement = true;
        decoding.displacement_size = 32;
        decoding.memory_address = read_dword(rip + 1);
        instruction_length += 4;
    } else if (decoding.mod == 1) {
        // [reg + disp8]
        decoding.has_displacement = true;
        decoding.displacement_size = 8;
        instruction_length += 1;
    } else if (decoding.mod == 2 || (decoding.mod == 0 && decoding.rm != 5)) {
        // [reg + disp32] 或 [reg]
        decoding.has_displacement = true;
        decoding.displacement_size = 32;
        instruction_length += 4;
    }
    
    // 判断是否为内存操作数
    decoding.is_memory_operand = (decoding.mod == 0) || (decoding.mod == 1) || (decoding.mod == 2);
    
    return decoding;
}

uint64_t X86CPUVM::calculate_effective_address(const ModRMDecoding& decoding, uint64_t rip) {
    if (!decoding.is_memory_operand) {
        return 0;  // 寄存器操作数
    }
    
    // 简化的有效地址计算
    // TODO: 完整的 SIB 和解码
    
    switch (decoding.rm) {
        case 0:  // [RAX]
            return get_register(X86Reg::RAX) + (decoding.has_displacement ? decoding.memory_address : 0);
        case 1:  // [RCX]
            return get_register(X86Reg::RCX) + (decoding.has_displacement ? decoding.memory_address : 0);
        case 2:  // [RDX]
            return get_register(X86Reg::RDX) + (decoding.has_displacement ? decoding.memory_address : 0);
        case 3:  // [RBX]
            return get_register(X86Reg::RBX) + (decoding.has_displacement ? decoding.memory_address : 0);
        case 4:  // [RSP] 或 SIB
            return get_register(X86Reg::RSP) + (decoding.has_displacement ? decoding.memory_address : 0);
        case 5:  // [RBP] 或 disp32
            if (decoding.mod == 0) {
                return decoding.memory_address;  // 直接地址
            }
            return get_register(X86Reg::RBP) + (decoding.has_displacement ? decoding.memory_address : 0);
        case 6:  // [RSI]
            return get_register(X86Reg::RSI) + (decoding.has_displacement ? decoding.memory_address : 0);
        case 7:  // [RDI]
            return get_register(X86Reg::RDI) + (decoding.has_displacement ? decoding.memory_address : 0);
        default:
            return 0;
    }
}

// ===== 主解码执行函数 =====
int X86CPUVM::decode_and_execute() {
    uint64_t rip = get_rip();
    
    // 读取第一个字节（可能是前缀或操作码）
    uint8_t first_byte = read_byte(rip);
    
    // 检查指令前缀
    int prefix_bytes = 0;
    
    // 处理 REX 前缀（x86-64 新增，0x40-0x4F）
    if ((first_byte & 0xF0) == 0x40) {
        // REX 前缀：0100WRXB
        prefix_bytes++;
        rip++;
        first_byte = read_byte(rip);
    }
    
    // 处理其他前缀
    while (first_byte == 0x66 || first_byte == 0x67 ||  // 操作数大小/地址大小前缀
           (first_byte >= 0xF0 && first_byte <= 0xF3)) {  // LOCK/REP 前缀
        prefix_bytes++;
        rip++;
        first_byte = read_byte(rip);
    }
    
    // 根据第一个字节分发指令
    switch (first_byte) {
        // ===== MOV 指令族 =====
        case 0x88:  // MOV r/m8, r8
        case 0x89:  // MOV r/m64, r64
        case 0x8A:  // MOV r8, r/m8
        case 0x8B:  // MOV r64, r/m64
        case 0xB8: case 0xB9: case 0xBA: case 0xBB:  // MOV r64, imm64
        case 0xBC: case 0xBD: case 0xBE: case 0xBF:  // MOV r64, imm64
        case 0xC6:  // MOV r/m8, imm8
        case 0xC7:  // MOV r/m64, imm64
            return execute_mov(rip);
        
        // ===== PUSH/POP =====
        case 0x50: case 0x51: case 0x52: case 0x53:  // PUSH RAX-RCX-RDX-RBX
        case 0x54: case 0x55: case 0x56: case 0x57:  // PUSH RSP-RBP-RSI-RDI
        case 0x58: case 0x59: case 0x5A: case 0x5B:  // POP RAX-RCX-RDX-RBX
        case 0x5C: case 0x5D: case 0x5E: case 0x5F:  // POP RSP-RBP-RSI-RDI
        case 0x68:  // PUSH imm32
        case 0x6A:  // PUSH imm8
        case 0x69:  // POP imm32
            return execute_push_pop(rip);
        
        // ===== 算术运算 =====
        case 0x00: case 0x01: case 0x02: case 0x03:  // ADD
        case 0x28: case 0x29: case 0x2A: case 0x2B:  // SUB
            return execute_arithmetic(rip);
        
        // ===== 逻辑运算 =====
        case 0xA8: case 0xA9:  // TEST
        case 0x08: case 0x09:  // OR
        case 0x20: case 0x21:  // AND
        case 0x30: case 0x31:  // XOR
        case 0xF6: case 0xF7:  // NOT/NEG
            return execute_logical(rip);
        
        // ===== 跳转指令 =====
        case 0xEB:  // JMP rel8
        case 0xE9:  // JMP rel32
        case 0x70: case 0x71: case 0x72: case 0x73:  // JO/JNO/JB/JAE
        case 0x74: case 0x75: case 0x76: case 0x77:  // JZ/JNZ/JBE/JA
        case 0x78: case 0x79: case 0x7A: case 0x7B:  // JS/JNS/JP/JNP
        case 0x7C: case 0x7D: case 0x7E: case 0x7F:  // JL/JGE/JLE/JG
        case 0xE3:  // JCXZ/JECXZ/JRCXZ
            return execute_branch(rip);
        
        // ===== CALL/RET =====
        case 0xE8:  // CALL rel32
        case 0xFF:  // CALL/RET (需要 ModR/M 解码)
            return execute_call_ret(rip);
        
        // ===== 栈操作 =====
        case 0x9D:  // POPF
        case 0x9C:  // PUSHF
            return execute_stack_ops(rip);
        
        // ===== 标志位操作 =====
        case 0xF5:  // CMC
        case 0xF8:  // CLC
        case 0xF9:  // STC
            return execute_flag_ops(rip);
        
        // ===== 中断指令 =====
        case 0xCC:  // INT 3 (断点)
        case 0xCD:  // INT imm8
            return execute_interrupt(rip);
        
        // ===== 字符串操作 =====
        case 0xA4:  // MOVS
        case 0xAA:  // STOS
        case 0xAB:  // STOS
            return execute_string_ops(rip);
        
        // ===== NOP =====
        case 0x90:  // NOP
            return 1;
        
        // ===== HALT =====
        case 0xF4:  // HLT
            state_ = X86VMState::HALTED;
            running_ = false;
            std::cout << "[X86VM-" << vm_id_ << "] Halted at RIP=0x" 
                      << std::hex << get_rip() << std::dec << std::endl;
            return 1;
        
        // ===== SYSCALL =====
        case 0x05:  // SYSCALL (简化实现，使用 0x05 作为 syscall 操作码)
            {
                // 系统调用号从 RAX 寄存器获取
                uint64_t syscall_num = get_register(X86Reg::RAX);
                std::cout << "[X86VM-" << vm_id_ << "] SYSCALL triggered with num=" 
                          << syscall_num << " at RIP=0x" 
                          << std::hex << get_rip() << std::dec << std::endl;
                
                // 触发系统调用中断（使用特定的中断向量，例如 0x80）
                trigger_syscall(syscall_num);
                return 1;
            }
        
        default:
            std::cerr << "[X86VM-" << vm_id_ << "] Unknown opcode: 0x" 
                      << std::hex << static_cast<int>(first_byte) << std::dec
                      << " at RIP=0x" << get_rip() << std::endl;
            
            // 触发未定义指令异常（中断向量 #UD = 6）
            trigger_interrupt(6);
            return 1;
    }
}
