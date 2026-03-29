#include "X86CPU.h"
#include <iostream>

// ===== MOV 指令实现 =====
int X86CPUVM::execute_mov(uint64_t rip) {
    uint8_t opcode = read_byte(rip);
    int instr_len = 1;
    
    switch (opcode) {
        // MOV r64, imm64 (操作码 0xB8-0xBF)
        case 0xB8: case 0xB9: case 0xBA: case 0xBB:
        case 0xBC: case 0xBD: case 0xBE: case 0xBF: {
            X86Reg reg = static_cast<X86Reg>((opcode - 0xB8));
            uint64_t imm_val = read_qword(rip + 1);
            set_register(reg, imm_val);
            instr_len = 8;
            break;
        }
        
        case 0x8B: {  // MOV r64, r/m64
            ModRMDecoding decoding = decode_modrm(rip + 1, instr_len);
            
            if (!decoding.is_memory_operand) {
                // 寄存器到寄存器
                uint64_t src_val = get_register(static_cast<X86Reg>(decoding.rm));
                set_register(static_cast<X86Reg>(decoding.reg), src_val);
            } else {
                // 内存到寄存器
                uint64_t addr = calculate_effective_address(decoding, rip + 1);
                uint64_t src_val = read_qword(addr);
                set_register(static_cast<X86Reg>(decoding.reg), src_val);
            }
            break;
        }
        
        case 0x89: {  // MOV r/m64, r64
            ModRMDecoding decoding = decode_modrm(rip + 1, instr_len);
            
            uint64_t src_val = get_register(static_cast<X86Reg>(decoding.reg));
            
            if (!decoding.is_memory_operand) {
                // 寄存器到寄存器
                set_register(static_cast<X86Reg>(decoding.rm), src_val);
            } else {
                // 寄存器到内存
                uint64_t addr = calculate_effective_address(decoding, rip + 1);
                write_qword(addr, src_val);
            }
            break;
        }
        
        case 0xC7: {  // MOV r/m64, imm64
            ModRMDecoding decoding = decode_modrm(rip + 1, instr_len);
            uint64_t imm_val = read_qword(rip + 1 + instr_len);
            instr_len += 8;
            
            if (!decoding.is_memory_operand) {
                set_register(static_cast<X86Reg>(decoding.rm), imm_val);
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
        case 0x01: {  // ADD r/m64, r64
            ModRMDecoding decoding = decode_modrm(rip + 1, instr_len);
            
            uint64_t dest_val, src_val;
            
            if (!decoding.is_memory_operand) {
                dest_val = get_register(static_cast<X86Reg>(decoding.rm));
                src_val = get_register(static_cast<X86Reg>(decoding.reg));
                
                uint64_t result = dest_val + src_val;
                set_register(static_cast<X86Reg>(decoding.rm), result);
                update_flags_arithmetic(result, dest_val, src_val, true);
            } else {
                uint64_t addr = calculate_effective_address(decoding, rip + 1);
                dest_val = read_qword(addr);
                src_val = get_register(static_cast<X86Reg>(decoding.reg));
                
                uint64_t result = dest_val + src_val;
                write_qword(addr, result);
                update_flags_arithmetic(result, dest_val, src_val, true);
            }
            break;
        }
        
        case 0x03: {  // ADD r64, r/m64
            ModRMDecoding decoding = decode_modrm(rip + 1, instr_len);
            
            uint64_t dest_val = get_register(static_cast<X86Reg>(decoding.reg));
            uint64_t src_val;
            
            if (!decoding.is_memory_operand) {
                src_val = get_register(static_cast<X86Reg>(decoding.rm));
            } else {
                uint64_t addr = calculate_effective_address(decoding, rip + 1);
                src_val = read_qword(addr);
            }
            
            uint64_t result = dest_val + src_val;
            set_register(static_cast<X86Reg>(decoding.reg), result);
            update_flags_arithmetic(result, dest_val, src_val, true);
            break;
        }
        
        case 0x29: {  // SUB r/m64, r64
            ModRMDecoding decoding = decode_modrm(rip + 1, instr_len);
            
            uint64_t dest_val, src_val;
            
            if (!decoding.is_memory_operand) {
                dest_val = get_register(static_cast<X86Reg>(decoding.rm));
                src_val = get_register(static_cast<X86Reg>(decoding.reg));
            } else {
                uint64_t addr = calculate_effective_address(decoding, rip + 1);
                dest_val = read_qword(addr);
                src_val = get_register(static_cast<X86Reg>(decoding.reg));
            }
            
            uint64_t result = dest_val - src_val;
            
            if (!decoding.is_memory_operand) {
                set_register(static_cast<X86Reg>(decoding.rm), result);
            } else {
                write_qword(calculate_effective_address(decoding, rip + 1), result);
            }
            
            update_flags_arithmetic(result, dest_val, src_val, true);
            break;
        }
        
        case 0xF8:  // CLC
            set_flag(FLAG_CF, false);
            return 1;
        
        case 0xF9:  // STC
            set_flag(FLAG_CF, true);
            return 1;
        
        default:
            std::cerr << "[X86VM-" << vm_id_ << "] Unknown arithmetic opcode: 0x" 
                      << std::hex << static_cast<int>(opcode) << std::dec << std::endl;
            return 1;
    }
    
    return 1 + instr_len;
}
