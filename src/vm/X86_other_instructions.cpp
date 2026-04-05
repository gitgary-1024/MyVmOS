#include "X86CPU.h"
#include <iostream>

// ===== 逻辑运算实现 =====
int X86CPUVM::execute_logical(uint64_t rip) {
    uint8_t opcode = read_byte(rip);
    
    switch (opcode) {
        case 0xA9: {  // TEST RAX, imm32
            uint32_t imm = read_dword(rip + 1);
            uint64_t result = get_register(X86Reg::RAX) & imm;
            
            update_flags_arithmetic(result, get_register(X86Reg::RAX), imm, false);
            return 5;
        }
        
        case 0xF7: {  // TEST/NOT/NEG (需要 ModR/M)
            int len = 1;
            ModRMDecoding decoding = decode_modrm(rip + 1, len);
            
            uint8_t reg_field = decoding.reg;
            
            if (reg_field == 0x0) {  // TEST
                uint64_t imm = read_qword(rip + 2 + len);
                uint64_t dest_val;
                
                if (!decoding.is_memory_operand) {
                    dest_val = get_register(static_cast<X86Reg>(decoding.rm));
                } else {
                    uint64_t addr = calculate_effective_address(decoding, rip + 2);
                    dest_val = read_qword(addr);
                }
                
                uint64_t result = dest_val & imm;
                update_flags_arithmetic(result, dest_val, imm, false);
                return 2 + len + 8;
            } else if (reg_field == 0x2) {  // NOT
                if (!decoding.is_memory_operand) {
                    uint64_t val = ~get_register(static_cast<X86Reg>(decoding.rm));
                    set_register(static_cast<X86Reg>(decoding.rm), val);
                } else {
                    uint64_t addr = calculate_effective_address(decoding, rip + 2);
                    uint64_t val = ~read_qword(addr);
                    write_qword(addr, val);
                }
                return 2 + len;
            } else if (reg_field == 0x3) {  // NEG
                uint64_t dest_val;
                
                if (!decoding.is_memory_operand) {
                    dest_val = get_register(static_cast<X86Reg>(decoding.rm));
                } else {
                    uint64_t addr = calculate_effective_address(decoding, rip + 2);
                    dest_val = read_qword(addr);
                }
                
                uint64_t result = -dest_val;
                update_flags_arithmetic(result, 0, dest_val, true);
                
                if (!decoding.is_memory_operand) {
                    set_register(static_cast<X86Reg>(decoding.rm), result);
                } else {
                    write_qword(calculate_effective_address(decoding, rip + 2), result);
                }
                return 2 + len;
            }
            break;
        }
        
        default:
            std::cerr << "[X86VM-" << get_vm_id() << "] Unknown logical opcode: 0x"
                      << std::hex << static_cast<int>(opcode) << std::dec << std::endl;
            return 1;
    }
    
    return 1;
}

// ===== 跳转指令实现 =====
int X86CPUVM::execute_branch(uint64_t rip) {
    uint8_t opcode = read_byte(rip);
    
    // JMP rel8 (0xEB)
    if (opcode == 0xEB) {
        int8_t offset = read_byte(rip + 1);
        set_rip(get_rip() + 2 + offset);  // 显式设置 RIP，与 JMP rel32 保持一致
        return 2;
    }
    
    // JMP rel32 (0xE9)
    if (opcode == 0xE9) {
        int32_t offset = read_dword(rip + 1);
        int instr_len = 5;
        set_rip(get_rip() + instr_len + offset);
        return instr_len;
    }
    
    // 条件跳转 (0x70-0x7F)
    if (opcode >= 0x70 && opcode <= 0x7F) {
        int8_t offset = read_byte(rip + 1);
        bool take_branch = false;
        
        uint8_t condition = opcode & 0xF;
        bool cf = get_flag(FLAG_CF);
        bool zf = get_flag(FLAG_ZF);
        bool sf = get_flag(FLAG_SF);
        bool of = get_flag(FLAG_OF);
        bool pf = get_flag(FLAG_PF);
        
        switch (condition) {
            case 0x0: take_branch = of; break;  // JO
            case 0x1: take_branch = !of; break;  // JNO
            case 0x2: take_branch = cf; break;  // JB/JNAE
            case 0x3: take_branch = !cf; break;  // JAE/JNB
            case 0x4: take_branch = zf; break;  // JZ/JE
            case 0x5: take_branch = !zf; break;  // JNZ/JNE
            case 0x6: take_branch = cf || zf; break;  // JBE/JNA
            case 0x7: take_branch = !(cf || zf); break;  // JA/JNBE
            case 0x8: take_branch = sf; break;  // JS
            case 0x9: take_branch = !sf; break;  // JNS
            case 0xA: take_branch = pf; break;  // JP/JPE
            case 0xB: take_branch = !pf; break;  // JNP/JPO
            case 0xC: take_branch = sf != of; break;  // JL/JNGE
            case 0xD: take_branch = sf == of; break;  // JGE/JNL
            case 0xE: take_branch = zf || (sf != of); break;  // JLE/JNG
            case 0xF: take_branch = !zf && (sf == of); break;  // JG/JNLE
        }
        
        if (take_branch) {
            set_rip(get_rip() + 2 + offset);
        } else {
            return 2;
        }
        return 2;
    }
    
    // JCXZ/JECXZ/JRCXZ (0xE3)
    if (opcode == 0xE3) {
        int8_t offset = read_byte(rip + 1);
        uint64_t rcx = get_register(X86Reg::RCX);
        
        if ((rcx & 0xFFFFFFFF) == 0) {  // 检查低 32 位
            set_rip(get_rip() + 2 + offset);
        } else {
            return 2;
        }
        return 2;
    }
    
    std::cerr << "[X86VM-" << get_vm_id() << "] Unknown branch opcode: 0x" 
              << std::hex << static_cast<int>(opcode) << std::dec << std::endl;
    return 1;
}

// ===== CALL/RET 实现 =====
int X86CPUVM::execute_call_ret(uint64_t rip) {
    uint8_t opcode = read_byte(rip);
    
    // CALL rel32 (0xE8)
    if (opcode == 0xE8) {
        int32_t offset = read_dword(rip + 1);
        uint64_t ret_addr = get_rip() + 5;
        
        push(ret_addr);
        set_rip(get_rip() + 5 + offset);
        
        return 5;
    }
    
    // CALL/RET with ModR/M (0xFF)
    if (opcode == 0xFF) {
        int len = 1;
        ModRMDecoding decoding = decode_modrm(rip + 1, len);
        
        uint8_t reg_field = decoding.reg;
        
        if (reg_field == 0x2) {  // CALL near
            uint64_t target_addr;
            
            if (!decoding.is_memory_operand) {
                target_addr = get_register(static_cast<X86Reg>(decoding.rm));
            } else {
                uint64_t addr = calculate_effective_address(decoding, rip + 2);
                target_addr = read_qword(addr);
            }
            
            uint64_t ret_addr = get_rip() + 2 + len;
            push(ret_addr);
            set_rip(target_addr);
            
            return 2 + len;
        } else if (reg_field == 0x4) {  // JMP near
            uint64_t target_addr;
            
            if (!decoding.is_memory_operand) {
                target_addr = get_register(static_cast<X86Reg>(decoding.rm));
            } else {
                uint64_t addr = calculate_effective_address(decoding, rip + 2);
                target_addr = read_qword(addr);
            }
            
            set_rip(target_addr);
            return 2 + len;
        } else if (reg_field == 0x6) {  // PUSH near
            uint64_t src_val;
            
            if (!decoding.is_memory_operand) {
                src_val = get_register(static_cast<X86Reg>(decoding.rm));
            } else {
                uint64_t addr = calculate_effective_address(decoding, rip + 2);
                src_val = read_qword(addr);
            }
            
            push(src_val);
            return 2 + len;
        }
    }
    
    std::cerr << "[X86VM-" << get_vm_id() << "] Unknown CALL/RET opcode: 0x" 
              << std::hex << static_cast<int>(opcode) << std::dec << std::endl;
    return 1;
}

// ===== 栈操作实现 =====
int X86CPUVM::execute_stack_ops(uint64_t rip) {
    uint8_t opcode = read_byte(rip);
    
    // POPF (0x9D)
    if (opcode == 0x9D) {
        uint64_t flags = pop();
        set_register(X86Reg::RFLAGS, flags);
        return 1;
    }
    
    // PUSHF (0x9C)
    if (opcode == 0x9C) {
        push(get_rflags());
        return 1;
    }
    
    std::cerr << "[X86VM-" << get_vm_id() << "] Unknown stack opcode: 0x" 
              << std::hex << static_cast<int>(opcode) << std::dec << std::endl;
    return 1;
}

// ===== 标志位操作实现 =====
int X86CPUVM::execute_flag_ops(uint64_t rip) {
    uint8_t opcode = read_byte(rip);
    
    switch (opcode) {
        case 0xF5:  // CMC (Complement Carry Flag)
            set_flag(FLAG_CF, !get_flag(FLAG_CF));
            return 1;
        
        case 0xF8:  // CLC (Clear Carry Flag)
            set_flag(FLAG_CF, false);
            return 1;
        
        case 0xF9:  // STC (Set Carry Flag)
            set_flag(FLAG_CF, true);
            return 1;
        
        default:
            std::cerr << "[X86VM-" << get_vm_id() << "] Unknown flag opcode: 0x"
                      << std::hex << static_cast<int>(opcode) << std::dec << std::endl;
            return 1;
    }
}

// ===== 中断指令实现 =====
int X86CPUVM::execute_interrupt(uint64_t rip) {
    uint8_t opcode = read_byte(rip);
    
    // INT 3 (断点) (0xCC)
    if (opcode == 0xCC) {
        std::cout << "[X86VM-" << get_vm_id() << "] INT3 breakpoint at RIP=0x"
                  << std::hex << get_rip() << std::dec << std::endl;
        
        trigger_interrupt(3);  // 断点异常
        return 1;
    }
    
    // INT imm8 (0xCD)
    if (opcode == 0xCD) {
        uint8_t vector = read_byte(rip + 1);
        std::cout << "[X86VM-" << get_vm_id() << "] INT 0x"
                  << std::hex << static_cast<int>(vector) << std::dec
                  << " at RIP=0x" << get_rip() << std::endl;
        
        trigger_interrupt(vector);
        return 2;
    }
    
    std::cerr << "[X86VM-" << get_vm_id() << "] Unknown interrupt opcode: 0x" 
              << std::hex << static_cast<int>(opcode) << std::dec << std::endl;
    return 1;
}

// ===== 字符串操作实现 =====
int X86CPUVM::execute_string_ops(uint64_t rip) {
    uint8_t opcode = read_byte(rip);
    
    switch (opcode) {
        case 0xA4: {  // MOVS: Move byte from [RSI] to [RDI]
            uint8_t byte = read_byte(get_register(X86Reg::RSI));
            write_byte(get_register(X86Reg::RDI), byte);
            
            // 根据 DF 标志更新指针
            if (get_flag(FLAG_DF)) {
                set_register(X86Reg::RSI, get_register(X86Reg::RSI) - 1);
                set_register(X86Reg::RDI, get_register(X86Reg::RDI) - 1);
            } else {
                set_register(X86Reg::RSI, get_register(X86Reg::RSI) + 1);
                set_register(X86Reg::RDI, get_register(X86Reg::RDI) + 1);
            }
            return 1;
        }
        
        case 0xAA: {  // STOS: Store AL to [RDI]
            uint8_t al = get_register(X86Reg::RAX) & 0xFF;
            write_byte(get_register(X86Reg::RDI), al);
            
            if (get_flag(FLAG_DF)) {
                set_register(X86Reg::RDI, get_register(X86Reg::RDI) - 1);
            } else {
                set_register(X86Reg::RDI, get_register(X86Reg::RDI) + 1);
            }
            return 1;
        }
        
        case 0xAB: {  // STOS: Store EAX/RAX to [RDI]
            write_qword(get_register(X86Reg::RDI), get_register(X86Reg::RAX));
            
            if (get_flag(FLAG_DF)) {
                set_register(X86Reg::RDI, get_register(X86Reg::RDI) - 8);
            } else {
                set_register(X86Reg::RDI, get_register(X86Reg::RDI) + 8);
            }
            return 1;
        }
        
        default:
            std::cerr << "[X86VM-" << get_vm_id() << "] Unknown string opcode: 0x"
                      << std::hex << static_cast<int>(opcode) << std::dec << std::endl;
            return 1;
    }
}
