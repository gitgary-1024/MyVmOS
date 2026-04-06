#pragma once

#include <cstdint>
#include <vector>
#include <cstddef>

namespace cfg {

/**
 * 反汇编追踪器（Disassembly Tracker）
 * 
 * 使用 vector<uint8_t> 追踪哪些地址已被作为入口点反汇编过。
 * 
 * 状态定义：
 * - UNPROCESSED (0): 未处理
 * - ENTRY_POINT (1): 显式指定的入口点
 * - JUMP_TARGET (2): 跳转发现的目标
 * - PROCESSED (3): 已反汇编
 */
class DisassemblyTracker {
public:
    enum State : uint8_t {
        UNPROCESSED = 0,   // 未处理
        ENTRY_POINT = 1,   // 显式指定的入口点
        JUMP_TARGET = 2,   // 跳转发现的目标
        PROCESSED = 3      // 已反汇编
    };
    
private:
    std::vector<uint8_t> state_;  // 每个字节的状态
    size_t code_size_;
    
public:
    /**
     * 构造函数
     * @param code_size 代码段大小
     */
    explicit DisassemblyTracker(size_t code_size) 
        : state_(code_size, UNPROCESSED), code_size_(code_size) {}
    
    /**
     * 检查地址是否有效
     */
    bool is_valid_address(uint64_t offset) const {
        return offset < code_size_;
    }
    
    /**
     * 获取地址的状态
     */
    State get_state(uint64_t offset) const {
        if (!is_valid_address(offset)) {
            return UNPROCESSED;
        }
        return static_cast<State>(state_[offset]);
    }
    
    /**
     * 检查是否是入口点（ENTRY_POINT 或 JUMP_TARGET 或 PROCESSED）
     */
    bool is_entry_point(uint64_t offset) const {
        if (!is_valid_address(offset)) {
            return false;
        }
        return state_[offset] >= ENTRY_POINT;
    }
    
    /**
     * 检查是否已处理
     */
    bool is_processed(uint64_t offset) const {
        if (!is_valid_address(offset)) {
            return false;
        }
        return state_[offset] == PROCESSED;
    }
    
    /**
     * 标记为入口点
     */
    void mark_as_entry(uint64_t offset) {
        if (is_valid_address(offset)) {
            state_[offset] = ENTRY_POINT;
        }
    }
    
    /**
     * 标记为跳转目标
     */
    void mark_as_jump_target(uint64_t offset) {
        if (is_valid_address(offset)) {
            state_[offset] = JUMP_TARGET;
        }
    }
    
    /**
     * 标记为已处理
     */
    void mark_as_processed(uint64_t offset) {
        if (is_valid_address(offset)) {
            state_[offset] = PROCESSED;
        }
    }
    
    /**
     * 检测冲突：同一地址被不同方式反汇编
     * 
     * 如果指令内部的某个字节已经被标记为 PROCESSED，则存在冲突。
     * 
     * @param offset 指令起始地址
     * @param instruction_size 指令长度
     * @return true 如果存在冲突
     */
    bool has_conflict(uint64_t offset, uint16_t instruction_size) const {
        for (uint16_t i = 1; i < instruction_size; i++) {
            if (offset + i < code_size_ && state_[offset + i] == PROCESSED) {
                return true;  // 指令内部有已处理的字节
            }
        }
        return false;
    }
    
    /**
     * 获取代码段大小
     */
    size_t code_size() const {
        return code_size_;
    }
    
    /**
     * 打印追踪器状态（调试用）
     */
    void print_summary() const {
        size_t unprocessed = 0, entry = 0, jump_target = 0, processed = 0;
        
        for (size_t i = 0; i < code_size_; i++) {
            switch (state_[i]) {
                case UNPROCESSED: unprocessed++; break;
                case ENTRY_POINT: entry++; break;
                case JUMP_TARGET: jump_target++; break;
                case PROCESSED: processed++; break;
            }
        }
        
        printf("=== DisassemblyTracker Summary ===\n");
        printf("  Code size: %zu bytes\n", code_size_);
        printf("  Unprocessed: %zu\n", unprocessed);
        printf("  Entry points: %zu\n", entry);
        printf("  Jump targets: %zu\n", jump_target);
        printf("  Processed: %zu\n", processed);
        printf("==================================\n");
    }
};

} // namespace cfg
