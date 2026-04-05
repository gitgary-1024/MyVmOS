#include <iostream>
#include <iomanip>
#include "vm/disassembly/CapstoneDisassembler.h"
#include "vm/disassembly/x86/X86Instruction.h"

using namespace disassembly;

int main() {
    system("chcp 65001 > nul");
    std::cout << "=== Testing New Capstone-based Disassembly System ===" << std::endl;
    
    try {
        // 创建 x86-64 反汇编器
        CapstoneDisassembler disasm(Architecture::X86, Mode::MODE_64);
        
        std::cout << "Capstone version: " << disasm.get_version() << std::endl;
        
        // 测试用例 1：简单的 32 位指令
        std::cout << "\n--- Test 1: Simple 32-bit instructions ---" << std::endl;
        uint8_t code1[] = {
            0xB8, 0x05, 0x00, 0x00, 0x00,  // MOV EAX, 5
            0xBB, 0x03, 0x00, 0x00, 0x00,  // MOV EBX, 3
            0x01, 0xD8,                     // ADD EAX, EBX
            0xF4                            // HLT
        };
        
        auto stream1 = disasm.disassemble(code1, sizeof(code1), 0x100000);
        
        std::cout << "Disassembled " << stream1->size() << " instructions:" << std::endl;
        for (size_t i = 0; i < stream1->size(); i++) {
            auto insn = (*stream1)[i];
            
            std::cout << "  [" << i << "] 0x" << std::hex << std::setw(8) << std::setfill('0')
                      << insn->address << ":  ";
            
            // 打印机器码
            for (uint16_t j = 0; j < insn->size; j++) {
                std::cout << std::hex << std::setw(2) << std::setfill('0') 
                          << static_cast<int>(code1[insn->address - 0x100000 + j]) << " ";
            }
            
            std::cout << std::dec << "\t" << insn->mnemonic << "\t" << insn->operands_str << std::endl;
            
            // 显示指令类别
            std::string category_str;
            switch (insn->category) {
                case InstructionCategory::DATA_TRANSFER: category_str = "DATA_TRANSFER"; break;
                case InstructionCategory::ARITHMETIC: category_str = "ARITHMETIC"; break;
                case InstructionCategory::CONTROL_FLOW: category_str = "CONTROL_FLOW"; break;
                case InstructionCategory::HALT: category_str = "HALT"; break;
                default: category_str = "OTHER"; break;
            }
            std::cout << "        Category: " << category_str << std::endl;
            
            // 显示操作数详情
            std::cout << "        Operands (" << insn->operands.size() << "): ";
            for (const auto& op : insn->operands) {
                std::cout << "[" << op->to_string() << "] ";
            }
            std::cout << std::endl;
            
            // 访问 x86 特有数据（组合模式，使用 shared_ptr）
            if (insn->arch_specific_data) {
                auto x86_data = std::static_pointer_cast<x86::X86InstructionData>(
                    insn->arch_specific_data);
                std::cout << "        REX prefix: " << (x86_data->prefix.has_rex ? "Yes" : "No") << std::endl;
                std::cout << "        Opcode: 0x" << std::hex << std::setw(2) << std::setfill('0')
                          << static_cast<int>(x86_data->opcode[0]) << std::dec << std::endl;
            }
        }
        
        // 测试用例 2：带 REX 前缀的 64 位指令
        std::cout << "\n--- Test 2: 64-bit instructions with REX prefix ---" << std::endl;
        uint8_t code2[] = {
            0x48, 0xC7, 0xC0, 0x05, 0x00, 0x00, 0x00,  // MOV RAX, 5
            0x48, 0xC7, 0xC3, 0x03, 0x00, 0x00, 0x00,  // MOV RBX, 3
            0x48, 0x01, 0xD8,                           // ADD RAX, RBX
            0xF4                                        // HLT
        };
        
        auto stream2 = disasm.disassemble(code2, sizeof(code2), 0x100000);
        
        std::cout << "Disassembled " << stream2->size() << " instructions:" << std::endl;
        for (size_t i = 0; i < stream2->size(); i++) {
            auto insn = (*stream2)[i];
            
            std::cout << "  [" << i << "] 0x" << std::hex << std::setw(8) << std::setfill('0')
                      << insn->address << ":  "
                      << insn->mnemonic << "\t" << insn->operands_str << std::dec << std::endl;
            
            // 访问 x86 特有数据（使用 shared_ptr）
            if (insn->arch_specific_data) {
                auto x86_data = std::static_pointer_cast<x86::X86InstructionData>(
                    insn->arch_specific_data);
                std::cout << "        Has REX: " << (x86_data->prefix.has_rex ? "Yes" : "No");
                if (x86_data->prefix.has_rex) {
                    std::cout << " (value=0x" << std::hex << std::setw(2) << std::setfill('0')
                              << static_cast<int>(x86_data->prefix.rex_value) << ")";
                }
                std::cout << std::dec << std::endl;
                std::cout << "        Is 64-bit: " << (x86_data->is_64bit() ? "Yes" : "No") << std::endl;
            }
        }
        
        std::cout << "\n=== Disassembly System Test Complete ===" << std::endl;
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
}
