#pragma once

#include <cstdint>
#include <vector>
#include <memory>
#include <string>

namespace cfg {

/**
 * 简化的指令表示（用于 CFG）
 * 
 * 注意：这是简化版本，实际项目中应该使用完整的 Instruction 类
 */
struct SimpleInstruction {
    uint64_t address;      // 指令地址
    uint16_t size;         // 指令长度
    std::string mnemonic;  // 助记符（如 "MOV", "ADD"）
    std::string operands;  // 操作数（如 "RAX, RBX"）
    
    // Capstone 详细数据支持
    void* capstone_insn;   // Capstone cs_insn 指针（void* 避免头文件依赖）
    bool has_capstone_data; // 标记是否有 Capstone 详细数据
    
    SimpleInstruction() 
        : address(0), size(0), capstone_insn(nullptr), has_capstone_data(false) {}
    SimpleInstruction(uint64_t addr, uint16_t sz, const std::string& mnem, const std::string& ops = "")
        : address(addr), size(sz), mnemonic(mnem), operands(ops), 
          capstone_insn(nullptr), has_capstone_data(false) {}
    
    ~SimpleInstruction() {
        // ⚠️ 注意：Capstone 内存需要手动管理
        // 如果需要释放，请在包含 capstone.h 后调用 cs_free
        // 这里不自动释放，避免头文件依赖问题
    }
};

/**
 * 基本块（Basic Block）
 * 
 * 定义：
 * - 单一入口（第一条指令）
 * - 单一出口（最后一条指令）
 * - 中间无跳转目标
 * - 中间无跳转指令
 * 
 * 划分点：
 * 1. 函数入口
 * 2. 跳转目标地址
 * 3. 跳转指令的下一条指令（fall-through）
 * 4. RET/HLT 指令后
 */
struct BasicBlock {
    uint64_t start_addr;   // 起始地址
    uint64_t end_addr;     // 结束地址（最后一条指令的末尾）
    
    // 指令列表
    std::vector<std::shared_ptr<SimpleInstruction>> instructions;
    
    // 控制流关系
    std::vector<uint64_t> successors;    // 后继块起始地址
    std::vector<uint64_t> predecessors;  // 前驱块起始地址
    
    // 属性
    bool is_terminated;      // 是否以 JMP/RET/HLT 结束
    bool is_entry_block;     // 是否是函数入口
    bool has_indirect_jmp;   // 是否包含间接跳转
    
    BasicBlock() 
        : start_addr(0), end_addr(0), 
          is_terminated(false), is_entry_block(false), 
          has_indirect_jmp(false) {}
    
    BasicBlock(uint64_t start) 
        : start_addr(start), end_addr(start),
          is_terminated(false), is_entry_block(false),
          has_indirect_jmp(false) {}
    
    /**
     * 添加指令到基本块
     */
    void add_instruction(std::shared_ptr<SimpleInstruction> insn) {
        instructions.push_back(insn);
        end_addr = insn->address + insn->size;
    }
    
    /**
     * 获取最后一条指令
     */
    const SimpleInstruction* last_instruction() const {
        return instructions.empty() ? nullptr : instructions.back().get();
    }
    
    /**
     * 获取指令数量
     */
    size_t instruction_count() const {
        return instructions.size();
    }
    
    /**
     * 添加后继块
     */
    void add_successor(uint64_t addr) {
        successors.push_back(addr);
    }
    
    /**
     * 添加前驱块
     */
    void add_predecessor(uint64_t addr) {
        predecessors.push_back(addr);
    }
    
    /**
     * 打印基本块信息（调试用）
     */
    void print() const {
        printf("BasicBlock [0x%llx - 0x%llx] (insns: %zu)\n", 
               start_addr, end_addr, instructions.size());
        printf("  Successors: ");
        for (auto succ : successors) {
            printf("0x%llx ", succ);
        }
        printf("\n");
        printf("  Predecessors: ");
        for (auto pred : predecessors) {
            printf("0x%llx ", pred);
        }
        printf("\n");
        printf("  Terminated: %s, Entry: %s, IndirectJmp: %s\n",
               is_terminated ? "yes" : "no",
               is_entry_block ? "yes" : "no",
               has_indirect_jmp ? "yes" : "no");
        
        for (const auto& insn : instructions) {
            printf("    0x%llx: %s %s\n", 
                   insn->address, 
                   insn->mnemonic.c_str(), 
                   insn->operands.c_str());
        }
    }
};

} // namespace cfg
