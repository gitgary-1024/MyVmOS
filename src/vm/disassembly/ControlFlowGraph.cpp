#include "ControlFlowGraph.h"
#include <iostream>
#include <algorithm>

namespace disassembly {

// ===== 公共方法实现 =====

void ControlFlowGraph::build(uint64_t entry_addr) {
    std::cout << "[CFG] Building control flow graph from entry point 0x" 
              << std::hex << entry_addr << std::dec << std::endl;
    
    // 1. 标记初始入口点
    tracker_.mark_as_entry(entry_addr);
    pending_entries_.insert(entry_addr);
    
    // 2. BFS 反汇编所有可达代码
    process_pending_entries();
    
    std::cout << "[CFG] CFG construction complete. Total blocks: " 
              << blocks_.size() << std::endl;
}

const BasicBlock* ControlFlowGraph::get_block(uint64_t addr) const {
    auto it = blocks_.find(addr);
    if (it != blocks_.end()) {
        return &(it->second);
    }
    return nullptr;
}

bool ControlFlowGraph::has_block(uint64_t addr) const {
    return blocks_.find(addr) != blocks_.end();
}

void ControlFlowGraph::for_each_block(std::function<void(const BasicBlock&)> callback) const {
    for (const auto& [addr, block] : blocks_) {
        callback(block);
    }
}

void ControlFlowGraph::print_summary() const {
    std::cout << "\n=== CFG Summary ===" << std::endl;
    std::cout << "Total basic blocks: " << blocks_.size() << std::endl;
    
    size_t total_instructions = 0;
    size_t terminated_blocks = 0;
    size_t entry_blocks = 0;
    
    for (const auto& [addr, block] : blocks_) {
        total_instructions += block.instruction_count();
        if (block.is_terminated) terminated_blocks++;
        if (block.is_entry_block) entry_blocks++;
    }
    
    std::cout << "Total instructions: " << total_instructions << std::endl;
    std::cout << "Terminated blocks: " << terminated_blocks << std::endl;
    std::cout << "Entry blocks: " << entry_blocks << std::endl;
    std::cout << "==================" << std::endl;
    
    tracker_.print_summary();
}

void ControlFlowGraph::print_all_blocks() const {
    std::cout << "\n=== All Basic Blocks ===" << std::endl;
    
    // 按地址排序输出
    std::vector<uint64_t> addresses;
    for (const auto& [addr, block] : blocks_) {
        addresses.push_back(addr);
    }
    std::sort(addresses.begin(), addresses.end());
    
    for (uint64_t addr : addresses) {
        auto it = blocks_.find(addr);
        if (it != blocks_.end()) {
            it->second.print();
            std::cout << std::endl;
        }
    }
    
    std::cout << "=====================" << std::endl;
}

// ===== 私有方法实现 =====

void ControlFlowGraph::process_pending_entries() {
    std::cout << "[CFG] Processing pending entries..." << std::endl;
    
    // 记录所有需要添加的边（源地址 -> 目标地址列表）
    std::unordered_map<uint64_t, std::vector<uint64_t>> pending_edges;
    
    while (!pending_entries_.empty()) {
        // 取出一个待处理的入口点
        auto it = pending_entries_.begin();
        uint64_t entry_addr = *it;
        pending_entries_.erase(it);
        
        // 如果已经处理过，跳过
        if (tracker_.is_processed(entry_addr)) {
            std::cout << "  [SKIP] 0x" << std::hex << entry_addr 
                      << " already processed" << std::dec << std::endl;
            continue;
        }
        
        // 如果已经有基本块，跳过
        if (has_block(entry_addr)) {
            std::cout << "  [SKIP] 0x" << std::hex << entry_addr 
                      << " already has a block" << std::dec << std::endl;
            continue;
        }
        
        std::cout << "  [PROCESS] Creating basic block at 0x" 
                  << std::hex << entry_addr << std::dec << std::endl;
        
        // 创建基本块（传入 pending_edges 用于收集边信息）
        BasicBlock block = create_basic_block(entry_addr, pending_edges);
        
        // 存储基本块
        blocks_[entry_addr] = block;
        
        // 标记为已处理
        tracker_.mark_as_processed(entry_addr);
        
        std::cout << "    Created block with " << block.instruction_count() 
                  << " instructions" << std::endl;
    }
    
    // 所有基本块创建完成后，统一添加控制流边
    std::cout << "[CFG] Adding control flow edges..." << std::endl;
    for (const auto& [from, targets] : pending_edges) {
        for (uint64_t to : targets) {
            add_edge(from, to);
        }
    }
}

BasicBlock ControlFlowGraph::create_basic_block(uint64_t start_addr, 
                                                std::unordered_map<uint64_t, std::vector<uint64_t>>& pending_edges) {
    BasicBlock block(start_addr);
    block.is_entry_block = true;
    
    uint64_t current_addr = start_addr;
    bool should_continue = true;
    
    while (should_continue) {
        // 检查地址有效性
        if (current_addr >= code_size_) {
            std::cout << "    [WARN] Address 0x" << std::hex << current_addr 
                      << " exceeds code size" << std::dec << std::endl;
            break;
        }
        
        // 检查是否已经是其他基本块的入口点
        if (current_addr != start_addr && tracker_.is_entry_point(current_addr)) {
            std::cout << "    [INFO] Reached existing entry point at 0x" 
                      << std::hex << current_addr << std::dec << std::endl;
            
            // 记录控制流边（稍后统一添加）
            pending_edges[start_addr].push_back(current_addr);
            
            // 基本块在此结束
            break;
        }
        
        // 反汇编一条指令
        auto insn = disassemble_instruction(current_addr);
        if (!insn) {
            std::cout << "    [WARN] Failed to disassemble at 0x" 
                      << std::hex << current_addr << std::dec << std::endl;
            break;
        }
        
        // 检测冲突
        if (tracker_.has_conflict(current_addr, insn->size)) {
            std::cout << "    [WARN] Conflict detected at 0x" 
                      << std::hex << current_addr << std::dec << std::endl;
            // 仍然添加指令，但标记警告
        }
        
        // 添加指令到基本块
        block.add_instruction(insn);
        
        // 标记该地址已被处理
        if (current_addr != start_addr) {
            tracker_.mark_as_processed(current_addr);
        }
        
        // 判断指令类型并决定下一步
        if (is_unconditional_jump(insn.get())) {
            // 无条件跳转：基本块终止
            block.is_terminated = true;
            
            // 提取跳转目标并添加到待处理队列
            uint64_t target = extract_jump_target(insn.get(), current_addr);
            if (target < code_size_) {
                tracker_.mark_as_jump_target(target);
                pending_entries_.insert(target);
                pending_edges[start_addr].push_back(target);  // 记录边
                
                std::cout << "    [JMP] Unconditional jump to 0x" 
                          << std::hex << target << std::dec << std::endl;
            }
            
            should_continue = false;
            
        } else if (is_conditional_jump(insn.get())) {
            // 条件跳转：基本块终止，有两个后继
            block.is_terminated = true;
            
            // 提取跳转目标
            uint64_t target = extract_jump_target(insn.get(), current_addr);
            if (target < code_size_) {
                tracker_.mark_as_jump_target(target);
                pending_entries_.insert(target);
                pending_edges[start_addr].push_back(target);  // 记录边
                
                std::cout << "    [COND_JMP] Conditional jump to 0x" 
                          << std::hex << target << std::dec << std::endl;
            }
            
            // Fall-through：顺序执行的下一条指令
            uint64_t fallthrough = current_addr + insn->size;
            if (fallthrough < code_size_) {
                tracker_.mark_as_jump_target(fallthrough);
                pending_entries_.insert(fallthrough);
                pending_edges[start_addr].push_back(fallthrough);  // 记录边
                
                std::cout << "    [FALLTHROUGH] Next instruction at 0x" 
                          << std::hex << fallthrough << std::dec << std::endl;
            }
            
            should_continue = false;
            
        } else if (is_terminator(insn.get())) {
            // RET/HLT：基本块终止
            block.is_terminated = true;
            
            std::cout << "    [TERMINATOR] Block terminated by " 
                      << insn->mnemonic << std::endl;
            
            should_continue = false;
            
        } else {
            // 普通指令：继续下一条
            current_addr += insn->size;
        }
    }
    
    return block;
}

void ControlFlowGraph::add_edge(uint64_t from, uint64_t to) {
    // 查找源块
    auto it_from = blocks_.find(from);
    if (it_from == blocks_.end()) {
        // 源块可能还未创建，稍后会在创建时添加
        return;
    }
    
    // 添加后继
    if (std::find(it_from->second.successors.begin(), 
                  it_from->second.successors.end(), to) == it_from->second.successors.end()) {
        it_from->second.add_successor(to);
    }
    
    // 查找目标块并添加前驱
    auto it_to = blocks_.find(to);
    if (it_to != blocks_.end()) {
        if (std::find(it_to->second.predecessors.begin(), 
                      it_to->second.predecessors.end(), from) == it_to->second.predecessors.end()) {
            it_to->second.add_predecessor(from);
        }
    }
}

// ===== 模拟反汇编（简化版）=====

std::shared_ptr<SimpleInstruction> ControlFlowGraph::disassemble_instruction(uint64_t addr) const {
    // 使用 Capstone 反汇编器
    return disassembler_.disassemble_instruction(code_, code_size_, addr);
}

bool ControlFlowGraph::is_unconditional_jump(const SimpleInstruction* insn) const {
    return CapstoneDisassembler::is_unconditional_jump(insn);
}

bool ControlFlowGraph::is_conditional_jump(const SimpleInstruction* insn) const {
    return CapstoneDisassembler::is_conditional_jump(insn);
}

bool ControlFlowGraph::is_terminator(const SimpleInstruction* insn) const {
    return CapstoneDisassembler::is_terminator(insn);
}

uint64_t ControlFlowGraph::extract_jump_target(const SimpleInstruction* insn, uint64_t current_addr) const {
    return CapstoneDisassembler::extract_jump_target(insn, current_addr);
}

ControlFlowGraph::CFGStats ControlFlowGraph::get_stats() const {
    CFGStats stats;
    stats.total_blocks = blocks_.size();
    
    // 计算总边数
    for (const auto& [addr, block] : blocks_) {
        stats.total_edges += block.successors.size();
    }
    
    // 收集所有入口点（没有前驱的块）
    std::set<uint64_t> all_targets;
    for (const auto& [addr, block] : blocks_) {
        for (uint64_t succ : block.successors) {
            all_targets.insert(succ);
        }
    }
    
    for (const auto& [addr, block] : blocks_) {
        if (all_targets.find(addr) == all_targets.end()) {
            stats.entry_points.insert(addr);
        }
    }
    
    return stats;
}

} // namespace disassembly
