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
    
    // SIB 字节处理
    decoding.has_sib = false;
    if (decoding.mod != 3 && decoding.rm == 4) {
        // 存在 SIB 字节
        decoding.has_sib = true;
        decoding.sib_byte = read_byte(rip + 1);
        decoding.scale = (decoding.sib_byte >> 6) & 0x3;
        decoding.index = (decoding.sib_byte >> 3) & 0x7;
        decoding.base = decoding.sib_byte & 0x7;
        instruction_length += 1;  // SIB 字节
    }
    
    // 位移计算
    if (decoding.mod == 0 && decoding.rm == 5) {
        // [disp32]
        decoding.has_displacement = true;
        decoding.displacement_size = 32;
        decoding.displacement_value = static_cast<int64_t>(read_dword(rip + instruction_length));
        instruction_length += 4;
    } else if (decoding.mod == 1) {
        // [reg + disp8]
        decoding.has_displacement = true;
        decoding.displacement_size = 8;
        decoding.displacement_value = static_cast<int64_t>(static_cast<int8_t>(read_byte(rip + instruction_length)));
        instruction_length += 1;
    } else if (decoding.mod == 2) {
        // [reg + disp32]
        decoding.has_displacement = true;
        decoding.displacement_size = 32;
        decoding.displacement_value = static_cast<int64_t>(read_dword(rip + instruction_length));
        instruction_length += 4;
    }
    // mod == 0 && rm != 5: [reg] 无位移
    
    // 判断是否为内存操作数
    decoding.is_memory_operand = (decoding.mod != 3);  // mod=3 是寄存器寻址
    
    return decoding;
}

uint64_t X86CPUVM::calculate_effective_address(const ModRMDecoding& decoding, uint64_t rip) {
    if (!decoding.is_memory_operand) {
        return 0;  // 寄存器操作数
    }
    
    uint64_t base_addr = 0;
    uint64_t index_addr = 0;
    int scale_factor = 1;
    
    // 处理 SIB 字节
    if (decoding.has_sib) {
        // 基址寄存器
        if (decoding.base != 5 || decoding.mod != 0) {  // 如果 base=5 且 mod=0，则没有基址
            switch (decoding.base) {
                case 0: base_addr = get_register(X86Reg::RAX); break;
                case 1: base_addr = get_register(X86Reg::RCX); break;
                case 2: base_addr = get_register(X86Reg::RDX); break;
                case 3: base_addr = get_register(X86Reg::RBX); break;
                case 4: base_addr = get_register(X86Reg::RSP); break;
                case 5: base_addr = get_register(X86Reg::RBP); break;
                case 6: base_addr = get_register(X86Reg::RSI); break;
                case 7: base_addr = get_register(X86Reg::RDI); break;
                default: base_addr = 0; break;
            }
        }
        
        // 索引寄存器
        if (decoding.index != 4) {  // index=4 表示无索引寄存器
            switch (decoding.index) {
                case 0: index_addr = get_register(X86Reg::RAX); break;
                case 1: index_addr = get_register(X86Reg::RCX); break;
                case 2: index_addr = get_register(X86Reg::RDX); break;
                case 3: index_addr = get_register(X86Reg::RBX); break;
                case 4: index_addr = 0; break;  // RSP 不能作为索引
                case 5: index_addr = get_register(X86Reg::RBP); break;
                case 6: index_addr = get_register(X86Reg::RSI); break;
                case 7: index_addr = get_register(X86Reg::RDI); break;
                default: index_addr = 0; break;
            }
            // 比例因子: 0->1, 1->2, 2->4, 3->8
            scale_factor = 1 << decoding.scale;
        }
    } else {
        // 无 SIB 字节，使用 rm 字段作为基址
        switch (decoding.rm) {
            case 0:  // [RAX]
                base_addr = get_register(X86Reg::RAX);
                break;
            case 1:  // [RCX]
                base_addr = get_register(X86Reg::RCX);
                break;
            case 2:  // [RDX]
                base_addr = get_register(X86Reg::RDX);
                break;
            case 3:  // [RBX]
                base_addr = get_register(X86Reg::RBX);
                break;
            case 4:  // [RSP] - 不应该到达这里，因为 rm=4 应该有 SIB
                base_addr = get_register(X86Reg::RSP);
                break;
            case 5:  // [RBP] 或 disp32
                if (decoding.mod == 0) {
                    // 直接地址（无基址寄存器）
                    return static_cast<uint64_t>(decoding.displacement_value);
                }
                base_addr = get_register(X86Reg::RBP);
                break;
            case 6:  // [RSI]
                base_addr = get_register(X86Reg::RSI);
                break;
            case 7:  // [RDI]
                base_addr = get_register(X86Reg::RDI);
                break;
            default:
                return 0;
        }
    }
    
    // 计算最终地址: base + index * scale + displacement
    uint64_t effective_addr = base_addr + (index_addr * scale_factor);
    
    if (decoding.has_displacement) {
        effective_addr += static_cast<uint64_t>(decoding.displacement_value);
    }
    
    return effective_addr;
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
            return prefix_bytes + execute_mov(rip);  // ✅ 加上前缀字节数
        
        // ===== PUSH/POP =====
        case 0x50: case 0x51: case 0x52: case 0x53:  // PUSH RAX-RCX-RDX-RBX
        case 0x54: case 0x55: case 0x56: case 0x57:  // PUSH RSP-RBP-RSI-RDI
        case 0x58: case 0x59: case 0x5A: case 0x5B:  // POP RAX-RCX-RDX-RBX
        case 0x5C: case 0x5D: case 0x5E: case 0x5F:  // POP RSP-RBP-RSI-RDI
        case 0x68:  // PUSH imm32
        case 0x6A:  // PUSH imm8
        case 0x69:  // POP imm32
            return prefix_bytes + execute_push_pop(rip);  // ✅ 加上前缀字节数
        
        // ===== 算术运算 =====
        case 0x00: case 0x01: case 0x02: case 0x03:  // ADD
        case 0x28: case 0x29: case 0x2A: case 0x2B:  // SUB
        case 0x38: case 0x39: case 0x3A: case 0x3B:  // CMP
        case 0x83:  // ADD/SUB/CMP r/m32, imm8
        case 0xFF:  // INC/DEC/CALL/JMP (Group 5)
            return prefix_bytes + execute_arithmetic(rip);  // ✅ 加上前缀字节数
        
        // ===== 逻辑运算 =====
        case 0xA8: case 0xA9:  // TEST
        case 0x08: case 0x09:  // OR
        case 0x20: case 0x21:  // AND
        case 0x30: case 0x31:  // XOR
        case 0xF6: case 0xF7:  // NOT/NEG
            return prefix_bytes + execute_logical(rip);  // ✅ 加上前缀字节数
        
        // ===== 跳转指令 =====
        case 0xEB:  // JMP rel8
        case 0xE9:  // JMP rel32
        case 0x70: case 0x71: case 0x72: case 0x73:  // JO/JNO/JB/JAE
        case 0x74: case 0x75: case 0x76: case 0x77:  // JZ/JNZ/JBE/JA
        case 0x78: case 0x79: case 0x7A: case 0x7B:  // JS/JNS/JP/JNP
        case 0x7C: case 0x7D: case 0x7E: case 0x7F:  // JL/JGE/JLE/JG
        case 0xE3:  // JCXZ/JECXZ/JRCXZ
            return prefix_bytes + execute_branch(rip);  // ✅ 加上前缀字节数
        
        // ===== CALL/RET =====
        case 0xE8:  // CALL rel32
        // case 0xFF(已经在139行出现了):  // CALL/RET (需要 ModR/M 解码)
            return prefix_bytes + execute_call_ret(rip);  // ✅ 加上前缀字节数
        
        // ===== 栈操作 =====
        case 0x9D:  // POPF
        case 0x9C:  // PUSHF
            return prefix_bytes + execute_stack_ops(rip);  // ✅ 加上前缀字节数
        
        // ===== 标志位操作 =====
        case 0xF5:  // CMC
        case 0xF8:  // CLC
        case 0xF9:  // STC
        case 0xFA:  // CLI
        case 0xFB:  // STI
            return prefix_bytes + execute_flag_ops(rip);  // ✅ 加上前缀字节数
        
        // ===== 中断指令 =====
        case 0xCC:  // INT 3 (断点)
        case 0xCD:  // INT imm8
            return prefix_bytes + execute_interrupt(rip);  // ✅ 加上前缀字节数
        
        // ===== 字符串操作 =====
        case 0xA4:  // MOVS
        case 0xAA:  // STOS
        case 0xAB:  // STOS
            return prefix_bytes + execute_string_ops(rip);  // ✅ 加上前缀字节数
        
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
                      << " at RIP=0x" << get_rip() 
                      << " (local rip=0x" << rip << ")" << std::endl;
            std::cerr.flush();
            
            // 触发未定义指令异常（中断向量 #UD = 6）
            trigger_interrupt(6);
            return 1;
    }
}
