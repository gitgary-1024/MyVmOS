#include "X86CPU.h"
#include <iostream>

// ===== MOV 指令实现 =====
int X86CPUVM::execute_mov(uint64_t rip) {
    uint8_t opcode = read_byte(rip);
    int instr_len = 1;
    
    switch (opcode) {
        // MOV r8, imm8 (操作码 0xB0-0xB7)
        case 0xB0: case 0xB1: case 0xB2: case 0xB3:
        case 0xB4: case 0xB5: case 0xB6: case 0xB7: {
            uint8_t reg_encoding = opcode - 0xB0;
            X86Reg reg = x86_reg_encoding_to_enum(reg_encoding);
            uint8_t imm_val = read_byte(rip + 1);
            // 只设置寄存器的低 8 位
            uint64_t old_val = get_register(reg);
            uint64_t new_val = (old_val & ~0xFFULL) | imm_val;
            set_register(reg, new_val);
            instr_len = 1;
            break;
        }
        
        // MOV r64, imm64 (操作码 0xB8-0xBF)
        // 注意：在 x86-64 中，MOV r32, imm32 会自动零扩展到 64 位
        case 0xB8: case 0xB9: case 0xBA: case 0xBB:
        case 0xBC: case 0xBD: case 0xBE: case 0xBF: {
            uint8_t reg_encoding = opcode - 0xB8;
            X86Reg reg = x86_reg_encoding_to_enum(reg_encoding);
            
            // 检查是否有 REX.W 前缀（64 位操作数）
            // 简化处理：默认使用 32 位立即数（4 字节）
            uint64_t imm_val = read_dword(rip + 1);  // 读取 32 位立即数
            set_register(reg, imm_val);  // 自动零扩展
            instr_len = 4;  // 立即数是 4 字节
            break;
        }
        
        case 0x8B: {  // MOV r64, r/m64
            ModRMDecoding decoding = decode_modrm(rip + 1, instr_len);
            
            if (!decoding.is_memory_operand) {
                // 寄存器到寄存器
                uint64_t src_val = get_register(x86_reg_encoding_to_enum(decoding.rm));
                set_register(x86_reg_encoding_to_enum(decoding.reg), src_val);
            } else {
                // 内存到寄存器
                uint64_t addr = calculate_effective_address(decoding, rip + 1);
                uint64_t src_val = read_qword(addr);
                set_register(x86_reg_encoding_to_enum(decoding.reg), src_val);
            }
            break;
        }
        
        case 0x89: {  // MOV r/m64, r64
            ModRMDecoding decoding = decode_modrm(rip + 1, instr_len);
            
            uint64_t src_val = get_register(x86_reg_encoding_to_enum(decoding.reg));
            
            if (!decoding.is_memory_operand) {
                // 寄存器到寄存器
                set_register(x86_reg_encoding_to_enum(decoding.rm), src_val);
            } else {
                // 寄存器到内存
                uint64_t addr = calculate_effective_address(decoding, rip + 1);
                write_qword(addr, src_val);
            }
            break;
        }
        
        case 0xC7: {  // MOV r/m64, imm32 (32位立即数，零扩展到64位)
            ModRMDecoding decoding = decode_modrm(rip + 1, instr_len);
            uint32_t imm32 = read_dword(rip + 1 + instr_len);
            uint64_t imm_val = static_cast<uint64_t>(imm32);  // 零扩展
            instr_len += 4;
            
            if (!decoding.is_memory_operand) {
                set_register(x86_reg_encoding_to_enum(decoding.rm), imm_val);
            } else {
                uint64_t addr = calculate_effective_address(decoding, rip + 1);
                write_qword(addr, imm_val);
            }
            break;
        }
        
        default:
            std::cerr << "[X86VM-" << vm_id_ << "] Unknown MOV opcode: 0x" 
                      << std::hex << static_cast<int>(opcode) << std::dec << std::endl;
            return 1;
    }
    
    return 1 + instr_len;
}

// ===== PUSH/POP 实现 =====
int X86CPUVM::execute_push_pop(uint64_t rip) {
    uint8_t opcode = read_byte(rip);
    
    // PUSH RAX-RDI (0x50-0x5F)
    if (opcode >= 0x50 && opcode <= 0x5F) {
        X86Reg reg = static_cast<X86Reg>(opcode - 0x50);
        uint64_t val = get_register(reg);
        push(val);
        return 1;
    }
    
    // POP RAX-RDI (0x58-0x5F)
    if (opcode >= 0x58 && opcode <= 0x5F) {
        X86Reg reg = static_cast<X86Reg>(opcode - 0x58);
        uint64_t val = pop();
        set_register(reg, val);
        return 1;
    }
    
    // PUSH imm32 (0x68)
    if (opcode == 0x68) {
        uint32_t imm = read_dword(rip + 1);
        push(imm);
        return 5;
    }
    
    // PUSH imm8 (0x6A)
    if (opcode == 0x6A) {
        int8_t imm = read_byte(rip + 1);
        push(static_cast<uint64_t>(imm));
        return 2;
    }
    
    std::cerr << "[X86VM-" << vm_id_ << "] Unknown PUSH/POP opcode: 0x" 
              << std::hex << static_cast<int>(opcode) << std::dec << std::endl;
    return 1;
}

// ===== 算术运算实现 =====
int X86CPUVM::execute_arithmetic(uint64_t rip) {
    uint8_t opcode = read_byte(rip);
    int instr_len = 1;
    
    switch (opcode) {
        // ===== ADD 指令族 =====
        case 0x00: {  // ADD r/m8, r8 (8 位加法)
            ModRMDecoding decoding = decode_modrm(rip + 1, instr_len);
            
            uint8_t dest_val, src_val;
            
            if (!decoding.is_memory_operand) {
                // 寄存器到寄存器（8 位）
                dest_val = static_cast<uint8_t>(get_register(x86_reg_encoding_to_enum(decoding.rm)));
                src_val = static_cast<uint8_t>(get_register(x86_reg_encoding_to_enum(decoding.reg)));
                
                uint8_t result = dest_val + src_val;
                // 更新低 8 位
                uint64_t reg_val = get_register(x86_reg_encoding_to_enum(decoding.rm));
                reg_val = (reg_val & ~0xFFULL) | result;
                set_register(x86_reg_encoding_to_enum(decoding.rm), reg_val);
                update_flags_arithmetic(result, dest_val, src_val, true);
            } else {
                // 内存操作（8 位）
                uint64_t addr = calculate_effective_address(decoding, rip + 1);
                dest_val = read_byte(addr);
                src_val = static_cast<uint8_t>(get_register(x86_reg_encoding_to_enum(decoding.reg)));
                
                uint8_t result = dest_val + src_val;
                write_byte(addr, result);
                update_flags_arithmetic(result, dest_val, src_val, true);
            }
            break;
        }
        
        case 0x01: {  // ADD r/m64, r64
            ModRMDecoding decoding = decode_modrm(rip + 1, instr_len);
            
            uint64_t dest_val, src_val;
            
            if (!decoding.is_memory_operand) {
                dest_val = get_register(x86_reg_encoding_to_enum(decoding.rm));
                src_val = get_register(x86_reg_encoding_to_enum(decoding.reg));
                
                uint64_t result = dest_val + src_val;
                set_register(x86_reg_encoding_to_enum(decoding.rm), result);
                update_flags_arithmetic(result, dest_val, src_val, true);
            } else {
                uint64_t addr = calculate_effective_address(decoding, rip + 1);
                dest_val = read_qword(addr);
                src_val = get_register(x86_reg_encoding_to_enum(decoding.reg));
                
                uint64_t result = dest_val + src_val;
                write_qword(addr, result);
                update_flags_arithmetic(result, dest_val, src_val, true);
            }
            break;
        }
        
        case 0x83: {  // ADD/SUB/CMP r/m32, imm8 (带符号扩展的立即数)
            ModRMDecoding decoding = decode_modrm(rip + 1, instr_len);
            int8_t imm8 = static_cast<int8_t>(read_byte(rip + 1 + instr_len));
            uint64_t imm_val = static_cast<uint64_t>(static_cast<int64_t>(imm8));  // 符号扩展到 64 位
            instr_len += 1;  // 立即数字节
            
            uint64_t dest_val;
            if (!decoding.is_memory_operand) {
                dest_val = get_register(x86_reg_encoding_to_enum(decoding.rm));
            } else {
                uint64_t addr = calculate_effective_address(decoding, rip + 1);
                dest_val = read_qword(addr);
            }
            
            // 根据 reg 字段判断操作类型
            switch (decoding.reg) {
                case 0: {  // ADD
                    uint64_t result = dest_val + imm_val;
                    if (!decoding.is_memory_operand) {
                        set_register(x86_reg_encoding_to_enum(decoding.rm), result);
                    } else {
                        write_qword(calculate_effective_address(decoding, rip + 1), result);
                    }
                    update_flags_arithmetic(result, dest_val, imm_val, true);
                    break;
                }
                case 5: {  // SUB
                    uint64_t result = dest_val - imm_val;
                    if (!decoding.is_memory_operand) {
                        set_register(x86_reg_encoding_to_enum(decoding.rm), result);
                    } else {
                        write_qword(calculate_effective_address(decoding, rip + 1), result);
                    }
                    update_flags_arithmetic(result, dest_val, imm_val, false);
                    break;
                }
                case 7: {  // CMP
                    uint64_t result = dest_val - imm_val;
                    update_flags_arithmetic(result, dest_val, imm_val, false);
                    break;
                }
                default:
                    std::cerr << "[X86VM-" << vm_id_ << "] Unsupported 0x83 sub-opcode: reg=" 
                              << static_cast<int>(decoding.reg) << std::endl;
                    break;
            }
            break;
        }
        
        case 0x02: {  // ADD r8, r/m8 (8 位加法)
            ModRMDecoding decoding = decode_modrm(rip + 1, instr_len);
            
            uint8_t dest_val = static_cast<uint8_t>(get_register(static_cast<X86Reg>(decoding.reg)));
            uint8_t src_val;
            
            if (!decoding.is_memory_operand) {
                src_val = static_cast<uint8_t>(get_register(x86_reg_encoding_to_enum(decoding.rm)));
            } else {
                uint64_t addr = calculate_effective_address(decoding, rip + 1);
                src_val = read_byte(addr);
            }
            
            uint8_t result = dest_val + src_val;
            // 更新低 8 位
            uint64_t reg_val = get_register(x86_reg_encoding_to_enum(decoding.reg));
            reg_val = (reg_val & ~0xFFULL) | result;
            set_register(x86_reg_encoding_to_enum(decoding.reg), reg_val);
            update_flags_arithmetic(result, dest_val, src_val, true);
            break;
        }
        
        case 0x03: {  // ADD r64, r/m64
            ModRMDecoding decoding = decode_modrm(rip + 1, instr_len);
            
            uint64_t dest_val = get_register(x86_reg_encoding_to_enum(decoding.reg));
            uint64_t src_val;
            
            if (!decoding.is_memory_operand) {
                src_val = get_register(x86_reg_encoding_to_enum(decoding.rm));
            } else {
                uint64_t addr = calculate_effective_address(decoding, rip + 1);
                src_val = read_qword(addr);
            }
            
            uint64_t result = dest_val + src_val;
            set_register(x86_reg_encoding_to_enum(decoding.reg), result);
            update_flags_arithmetic(result, dest_val, src_val, true);
            break;
        }
        
        // ===== SUB 指令族 =====
        case 0x28: {  // SUB r/m8, r8 (8 位减法)
            ModRMDecoding decoding = decode_modrm(rip + 1, instr_len);
            
            uint8_t dest_val, src_val;
            
            if (!decoding.is_memory_operand) {
                dest_val = static_cast<uint8_t>(get_register(x86_reg_encoding_to_enum(decoding.rm)));
                src_val = static_cast<uint8_t>(get_register(x86_reg_encoding_to_enum(decoding.reg)));
                
                uint8_t result = dest_val - src_val;
                uint64_t reg_val = get_register(x86_reg_encoding_to_enum(decoding.rm));
                reg_val = (reg_val & ~0xFFULL) | result;
                set_register(x86_reg_encoding_to_enum(decoding.rm), reg_val);
                update_flags_arithmetic(result, dest_val, src_val, true);
            } else {
                uint64_t addr = calculate_effective_address(decoding, rip + 1);
                dest_val = read_byte(addr);
                src_val = static_cast<uint8_t>(get_register(x86_reg_encoding_to_enum(decoding.reg)));
                
                uint8_t result = dest_val - src_val;
                write_byte(addr, result);
                update_flags_arithmetic(result, dest_val, src_val, true);
            }
            break;
        }
        
        case 0x29: {  // SUB r/m64, r64
            ModRMDecoding decoding = decode_modrm(rip + 1, instr_len);
            
            uint64_t dest_val, src_val;
            
            if (!decoding.is_memory_operand) {
                dest_val = get_register(x86_reg_encoding_to_enum(decoding.rm));
                src_val = get_register(x86_reg_encoding_to_enum(decoding.reg));
            } else {
                uint64_t addr = calculate_effective_address(decoding, rip + 1);
                dest_val = read_qword(addr);
                src_val = get_register(x86_reg_encoding_to_enum(decoding.reg));
            }
            
            uint64_t result = dest_val - src_val;
            
            if (!decoding.is_memory_operand) {
                set_register(x86_reg_encoding_to_enum(decoding.rm), result);
            } else {
                write_qword(calculate_effective_address(decoding, rip + 1), result);
            }
            
            update_flags_arithmetic(result, dest_val, src_val, true);
            break;
        }
        
        case 0x2A: {  // SUB r8, r/m8 (8 位减法)
            ModRMDecoding decoding = decode_modrm(rip + 1, instr_len);
            
            uint8_t dest_val = static_cast<uint8_t>(get_register(x86_reg_encoding_to_enum(decoding.reg)));
            uint8_t src_val;
            
            if (!decoding.is_memory_operand) {
                src_val = static_cast<uint8_t>(get_register(x86_reg_encoding_to_enum(decoding.rm)));
            } else {
                uint64_t addr = calculate_effective_address(decoding, rip + 1);
                src_val = read_byte(addr);
            }
            
            uint8_t result = dest_val - src_val;
            uint64_t reg_val = get_register(x86_reg_encoding_to_enum(decoding.reg));
            reg_val = (reg_val & ~0xFFULL) | result;
            set_register(x86_reg_encoding_to_enum(decoding.reg), reg_val);
            update_flags_arithmetic(result, dest_val, src_val, true);
            break;
        }
        
        case 0x2B: {  // SUB r64, r/m64
            ModRMDecoding decoding = decode_modrm(rip + 1, instr_len);
            
            uint64_t dest_val = get_register(x86_reg_encoding_to_enum(decoding.reg));
            uint64_t src_val;
            
            if (!decoding.is_memory_operand) {
                src_val = get_register(x86_reg_encoding_to_enum(decoding.rm));
            } else {
                uint64_t addr = calculate_effective_address(decoding, rip + 1);
                src_val = read_qword(addr);
            }
            
            uint64_t result = dest_val - src_val;
            set_register(x86_reg_encoding_to_enum(decoding.reg), result);
            update_flags_arithmetic(result, dest_val, src_val, true);
            break;
        }
        
        case 0xF8:  // CLC
            set_flag(FLAG_CF, false);
            return 1;
        
        case 0xF9:  // STC
            set_flag(FLAG_CF, true);
            return 1;
        
        case 0xFF: {  // Group 5: INC/DEC/CALL/JMP/PUSH (r/m64)
            ModRMDecoding decoding = decode_modrm(rip + 1, instr_len);
            uint8_t reg_field = decoding.reg;  // 从 ModR/M 中提取 reg 字段
            
            switch (reg_field) {
                case 0: {  // INC r/m64
                    if (!decoding.is_memory_operand) {
                        uint64_t val = get_register(x86_reg_encoding_to_enum(decoding.rm));
                        uint64_t result = val + 1;
                        set_register(x86_reg_encoding_to_enum(decoding.rm), result);
                        // ✅ INC 需要更新标志位（除了 CF）
                        update_flags_arithmetic(result, val, 1, true);  // is_addition=true
                    } else {
                        uint64_t addr = calculate_effective_address(decoding, rip + 1);
                        uint64_t val = read_qword(addr);
                        uint64_t result = val + 1;
                        write_qword(addr, result);
                        // ✅ INC 需要更新标志位（除了 CF）
                        update_flags_arithmetic(result, val, 1, true);
                    }
                    break;
                }
                
                case 1: {  // DEC r/m64
                    if (!decoding.is_memory_operand) {
                        uint64_t val = get_register(x86_reg_encoding_to_enum(decoding.rm));
                        uint64_t result = val - 1;
                        set_register(x86_reg_encoding_to_enum(decoding.rm), result);
                        // ✅ DEC 需要更新标志位（除了 CF）
                        update_flags_arithmetic(result, val, 1, false);  // is_subtract=false, DEC 不影响 CF
                    } else {
                        uint64_t addr = calculate_effective_address(decoding, rip + 1);
                        uint64_t val = read_qword(addr);
                        uint64_t result = val - 1;
                        write_qword(addr, result);
                        // ✅ DEC 需要更新标志位（除了 CF）
                        update_flags_arithmetic(result, val, 1, false);
                    }
                    break;
                }
                
                default:
                    std::cerr << "[X86VM-" << vm_id_ << "] Unsupported 0xFF sub-opcode: reg=" 
                              << static_cast<int>(reg_field) << std::endl;
                    break;
            }
            break;
        }
        
        // ===== CMP 指令族 (0x38-0x3D, 0x80/0x81/0x83) =====
        case 0x39: {  // CMP r/m64, r64
            ModRMDecoding decoding = decode_modrm(rip + 1, instr_len);
            uint64_t src_val = get_register(x86_reg_encoding_to_enum(decoding.reg));
            uint64_t dest_val;
            
            if (!decoding.is_memory_operand) {
                dest_val = get_register(x86_reg_encoding_to_enum(decoding.rm));
            } else {
                uint64_t addr = calculate_effective_address(decoding, rip + 1);
                dest_val = read_qword(addr);
            }
            
            // 计算但不保存结果，只更新标志位
            uint64_t result = dest_val - src_val;
            update_flags_arithmetic(result, dest_val, src_val, false);  // CMP 是无符号比较
            break;
        }
        
        case 0x3B: {  // CMP r64, r/m64
            ModRMDecoding decoding = decode_modrm(rip + 1, instr_len);
            uint64_t src_val;
            
            if (!decoding.is_memory_operand) {
                src_val = get_register(x86_reg_encoding_to_enum(decoding.rm));
            } else {
                uint64_t addr = calculate_effective_address(decoding, rip + 1);
                src_val = read_qword(addr);
            }
            
            uint64_t dest_val = get_register(x86_reg_encoding_to_enum(decoding.reg));
            uint64_t result = dest_val - src_val;
            update_flags_arithmetic(result, dest_val, src_val, false);
            break;
        }
        
        default:
            std::cerr << "[X86VM-" << vm_id_ << "] Unknown arithmetic opcode: 0x" 
                      << std::hex << static_cast<int>(opcode) << std::dec << std::endl;
            return 1;
    }
    
    return 1 + instr_len;
}
