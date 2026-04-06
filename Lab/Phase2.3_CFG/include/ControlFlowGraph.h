#pragma once

#include "BasicBlock.h"
#include "DisassemblyTracker.h"
#include "CapstoneDisassembler.h"
#include <unordered_map>
#include <set>
#include <vector>
#include <functional>
#include <memory>

namespace cfg {

// 使用 phase2_3 命名空间的 CapstoneDisassembler
using phase2_3::CapstoneDisassembler;

/**
 * 控制流图（Control Flow Graph, CFG）
 * 
 * 负责：
 * - 构建完整的 CFG
 * - 管理所有基本块
 * - BFS 反汇编算法
 * - 添加控制流边
 */
class ControlFlowGraph {
private:
    std::unordered_map<uint64_t, BasicBlock> blocks_;  // 地址 -> 基本块
    std::set<uint64_t> pending_entries_;               // BFS 队列（待处理的入口点）
    DisassemblyTracker tracker_;                       // 入口点追踪器
    const uint8_t* code_;                              // 代码段指针
    size_t code_size_;                                 // 代码段大小
    CapstoneDisassembler disassembler_;                // Capstone 反汇编器
    
public:
    /**
     * 构造函数
     * @param code 代码段指针
     * @param code_size 代码段大小
     */
    ControlFlowGraph(const uint8_t* code, size_t code_size)
        : tracker_(code_size), code_(code), code_size_(code_size) {}
    
    /**
     * 构建 CFG
     * @param entry_addr 入口地址（相对于代码段起始的偏移）
     */
    void build(uint64_t entry_addr);
    
    /**
     * 获取指定地址的基本块
     * @param addr 地址
     * @return 基本块指针，如果不存在返回 nullptr
     */
    const BasicBlock* get_block(uint64_t addr) const;
    
    /**
     * 检查是否存在指定地址的基本块
     */
    bool has_block(uint64_t addr) const;
    
    /**
     * 获取所有基本块
     */
    const std::unordered_map<uint64_t, BasicBlock>& get_all_blocks() const {
        return blocks_;
    }
    
    /**
     * 遍历所有基本块
     */
    void for_each_block(std::function<void(const BasicBlock&)> callback) const;
    
    /**
     * 获取基本块数量
     */
    size_t block_count() const {
        return blocks_.size();
    }
    
    /**
     * 打印 CFG 摘要（调试用）
     */
    void print_summary() const;
    
    /**
     * 打印所有基本块（调试用）
     */
    void print_all_blocks() const;
    
private:
    /**
     * BFS 反汇编：处理所有待处理的入口点
     */
    void process_pending_entries();
    
    /**
     * 创建基本块
     * @param start_addr 起始地址
     * @param pending_edges 用于收集控制流边（输出参数）
     * @return 创建的基本块
     */
    BasicBlock create_basic_block(uint64_t start_addr, 
                                  std::unordered_map<uint64_t, std::vector<uint64_t>>& pending_edges);
    
    /**
     * 添加控制流边
     * @param from 源块起始地址
     * @param to 目标块起始地址
     */
    void add_edge(uint64_t from, uint64_t to);
    
    /**
     * 模拟反汇编一条指令（简化版，实际应调用 Capstone）
     * @param addr 指令地址
     * @return 简化的指令对象
     */
    std::shared_ptr<SimpleInstruction> disassemble_instruction(uint64_t addr) const;
    
    /**
     * 判断是否是无条件跳转指令
     */
    bool is_unconditional_jump(const SimpleInstruction* insn) const;
    
    /**
     * 判断是否是条件跳转指令
     */
    bool is_conditional_jump(const SimpleInstruction* insn) const;
    
    /**
     * 判断是否是 RET/HLT 指令
     */
    bool is_terminator(const SimpleInstruction* insn) const;
    
    /**
     * 提取跳转目标地址
     * @param insn 指令指针
     * @param current_addr 当前指令地址（用于 RIP-relative 计算）
     */
    uint64_t extract_jump_target(const SimpleInstruction* insn, uint64_t current_addr) const;
};

} // namespace cfg
