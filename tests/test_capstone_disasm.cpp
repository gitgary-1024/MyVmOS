#include <iostream>
#include <vector>
#include <cstdint>
#include <iomanip>
#include "capstone/capstone.h"

// 打印十六进制字节
void print_hex_bytes(const uint8_t* data, size_t size) {
    for (size_t i = 0; i < size; i++) {
        std::cout << std::hex << std::setw(2) << std::setfill('0') 
                  << static_cast<int>(data[i]) << " ";
    }
    std::cout << std::dec << std::endl;
}

int main() {
    system("chcp 65001 > nul");
    std::cout << "=== Capstone Disassembler Test ===" << std::endl;
    
    // 初始化 Capstone
    csh handle;
    cs_err err = cs_open(CS_ARCH_X86, CS_MODE_64, &handle);
    if (err != CS_ERR_OK) {
        std::cerr << "Failed to initialize Capstone: " << cs_strerror(err) << std::endl;
        return 1;
    }
    
    // 启用详细模式
    cs_option(handle, CS_OPT_DETAIL, CS_OPT_ON);
    
    // 测试用例 1：简单的 MOV 和 ADD（无 REX 前缀）
    std::cout << "\n--- Test 1: Simple 32-bit instructions ---" << std::endl;
    uint8_t code1[] = {
        0xB8, 0x05, 0x00, 0x00, 0x00,  // MOV EAX, 5
        0xBB, 0x03, 0x00, 0x00, 0x00,  // MOV EBX, 3
        0x01, 0xD8,                     // ADD EAX, EBX
        0xF4                            // HLT
    };
    
    cs_insn* insn;
    size_t count = cs_disasm(handle, code1, sizeof(code1), 0x100000, 0, &insn);
    
    if (count > 0) {
        for (size_t i = 0; i < count; i++) {
            std::cout << "0x" << std::hex << std::setw(8) << std::setfill('0') 
                      << insn[i].address << ":  ";
            
            // 打印机器码
            for (uint16_t j = 0; j < insn[i].size; j++) {
                std::cout << std::hex << std::setw(2) << std::setfill('0') 
                          << static_cast<int>(insn[i].bytes[j]) << " ";
            }
            
            std::cout << std::dec << "\t" << insn[i].mnemonic << "\t" << insn[i].op_str << std::endl;
        }
        cs_free(insn, count);
    } else {
        std::cerr << "ERROR: Failed to disassemble code!" << std::endl;
    }
    
    // 测试用例 2：带 REX 前缀的 64 位指令
    std::cout << "\n--- Test 2: 64-bit instructions with REX prefix ---" << std::endl;
    uint8_t code2[] = {
        0x48, 0xC7, 0xC0, 0x05, 0x00, 0x00, 0x00,  // MOV RAX, 5 (REX.W + MOV r/m64, imm32)
        0x48, 0xC7, 0xC3, 0x03, 0x00, 0x00, 0x00,  // MOV RBX, 3
        0x48, 0x01, 0xD8,                           // ADD RAX, RBX
        0xF4                                        // HLT
    };
    
    count = cs_disasm(handle, code2, sizeof(code2), 0x100000, 0, &insn);
    
    if (count > 0) {
        for (size_t i = 0; i < count; i++) {
            std::cout << "0x" << std::hex << std::setw(8) << std::setfill('0') 
                      << insn[i].address << ":  ";
            
            // 打印机器码
            for (uint16_t j = 0; j < insn[i].size; j++) {
                std::cout << std::hex << std::setw(2) << std::setfill('0') 
                          << static_cast<int>(insn[i].bytes[j]) << " ";
            }
            
            std::cout << std::dec << "\t" << insn[i].mnemonic << "\t" << insn[i].op_str << std::endl;
            
            // 如果有详细信息，打印寄存器访问情况
            if (insn[i].detail) {
                if (insn[i].detail->regs_read_count > 0) {
                    std::cout << "  Reads: ";
                    for (uint8_t j = 0; j < insn[i].detail->regs_read_count; j++) {
                        std::cout << cs_reg_name(handle, insn[i].detail->regs_read[j]) << " ";
                    }
                    std::cout << std::endl;
                }
                if (insn[i].detail->regs_write_count > 0) {
                    std::cout << "  Writes: ";
                    for (uint8_t j = 0; j < insn[i].detail->regs_write_count; j++) {
                        std::cout << cs_reg_name(handle, insn[i].detail->regs_write[j]) << " ";
                    }
                    std::cout << std::endl;
                }
            }
        }
        cs_free(insn, count);
    } else {
        std::cerr << "ERROR: Failed to disassemble code!" << std::endl;
    }
    
    // 测试用例 3：用户提供的失败程序
    std::cout << "\n--- Test 3: User's failing program ---" << std::endl;
    uint8_t code3[] = {
        0xB8, 0x05, 0x00, 0x00, 0x00,  // MOV EAX, 5
        0xBB, 0x03, 0x00, 0x00, 0x00,  // MOV EBX, 3
        0x01, 0xD8,                     // ADD EAX, EBX
        0xF4                            // HLT
    };
    
    count = cs_disasm(handle, code3, sizeof(code3), 0x100000, 0, &insn);
    
    if (count > 0) {
        std::cout << "Expected instruction sequence:" << std::endl;
        for (size_t i = 0; i < count; i++) {
            std::cout << "  [" << i << "] 0x" << std::hex << std::setw(8) << std::setfill('0') 
                      << insn[i].address << ":  ";
            
            for (uint16_t j = 0; j < insn[i].size; j++) {
                std::cout << std::hex << std::setw(2) << std::setfill('0') 
                          << static_cast<int>(insn[i].bytes[j]) << " ";
            }
            
            std::cout << std::dec << "\t" << insn[i].mnemonic << "\t" << insn[i].op_str << std::endl;
        }
        cs_free(insn, count);
    }
    
    // 清理
    cs_close(&handle);
    
    std::cout << "\n=== Disassembly Complete ===" << std::endl;
    return 0;
}
