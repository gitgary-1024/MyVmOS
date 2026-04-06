#pragma once

#include <cstdint>
#include <unordered_map>
#include <set>
#include <vector>
#include <string>
#include <iostream>

namespace disassembly {

/**
 * @brief 函数调用图（Call Graph）
 * 
 * 记录函数之间的调用关系，支持：
 * - 追踪 caller -> callee 关系
 * - 检测递归调用
 * - 识别所有函数入口点
 */
struct CallGraph {
    // caller -> callees (谁调用了谁)
    std::unordered_map<uint64_t, std::set<uint64_t>> call_targets;
    
    // callee -> callers (谁被谁调用)
    std::unordered_map<uint64_t, std::set<uint64_t>> callers;
    
    // 所有函数的入口点
    std::set<uint64_t> function_entries;
    
    // 递归函数集合
    std::set<uint64_t> recursive_functions;
    
    /**
     * @brief 添加一条调用关系
     * @param caller_addr 调用者地址
     * @param callee_addr 被调用者地址
     */
    void add_call(uint64_t caller_addr, uint64_t callee_addr) {
        call_targets[caller_addr].insert(callee_addr);
        callers[callee_addr].insert(caller_addr);
        
        // 标记为函数入口
        function_entries.insert(callee_addr);
        
        // 检测直接递归
        if (caller_addr == callee_addr) {
            recursive_functions.insert(caller_addr);
        }
    }
    
    /**
     * @brief 获取某个函数的所有调用目标
     */
    const std::set<uint64_t>& get_callees(uint64_t func_addr) const {
        static const std::set<uint64_t> empty_set;
        auto it = call_targets.find(func_addr);
        return (it != call_targets.end()) ? it->second : empty_set;
    }
    
    /**
     * @brief 获取某个函数的所有调用者
     */
    const std::set<uint64_t>& get_callers(uint64_t func_addr) const {
        static const std::set<uint64_t> empty_set;
        auto it = callers.find(func_addr);
        return (it != callers.end()) ? it->second : empty_set;
    }
    
    /**
     * @brief 检查是否是递归函数
     */
    bool is_recursive(uint64_t func_addr) const {
        return recursive_functions.find(func_addr) != recursive_functions.end();
    }
    
    /**
     * @brief 检查是否是已知函数入口
     */
    bool is_function_entry(uint64_t addr) const {
        return function_entries.find(addr) != function_entries.end();
    }
    
    /**
     * @brief 获取所有函数入口点
     */
    const std::set<uint64_t>& get_all_entries() const {
        return function_entries;
    }
    
    /**
     * @brief 打印 Call Graph（调试用）
     */
    void print() const {
        std::cout << "\n=== Call Graph ===" << std::endl;
        std::cout << "Total functions: " << function_entries.size() << std::endl;
        std::cout << "Recursive functions: " << recursive_functions.size() << std::endl;
        
        for (uint64_t entry : function_entries) {
            std::cout << "\nFunction at 0x" << std::hex << entry << std::dec;
            
            if (is_recursive(entry)) {
                std::cout << " [RECURSIVE]";
            }
            
            auto it = call_targets.find(entry);
            if (it != call_targets.end() && !it->second.empty()) {
                std::cout << " calls:";
                for (uint64_t callee : it->second) {
                    std::cout << " 0x" << std::hex << callee << std::dec;
                }
            } else {
                std::cout << " (leaf function)";
            }
            
            auto caller_it = callers.find(entry);
            if (caller_it != callers.end() && !caller_it->second.empty()) {
                std::cout << "\n  Called by:";
                for (uint64_t caller : caller_it->second) {
                    std::cout << " 0x" << std::hex << caller << std::dec;
                }
            }
            
            std::cout << std::endl;
        }
        
        std::cout << "==================" << std::endl;
    }
};

} // namespace disassembly
