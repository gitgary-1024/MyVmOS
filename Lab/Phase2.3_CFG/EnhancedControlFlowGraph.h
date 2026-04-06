#pragma once

#include "../src/vm/disassembly/ControlFlowGraph.h"
#include "CallGraph.h"
#include <algorithm>

namespace disassembly {

/**
 * @brief 增强的控制流图（带 CALL/RET 分析）
 * 
 * 在原有 CFG 基础上增加：
 * - Call Graph 构建
 * - CALL 指令的双后继处理
 * - 递归检测
 */
class EnhancedControlFlowGraph : public ControlFlowGraph {
private:
    CallGraph call_graph_;  // 函数调用图
    
public:
    /**
     * @brief 构造函数
     */
    EnhancedControlFlowGraph(const uint8_t* code, size_t code_size)
        : ControlFlowGraph(code, code_size) {}
    
    /**
     * @brief 获取 Call Graph
     */
    const CallGraph& get_call_graph() const {
        return call_graph_;
    }
    
    /**
     * @brief 打印 Call Graph
     */
    void print_call_graph() const {
        call_graph_.print();
    }
    
protected:
    /**
     * @brief 重写 create_basic_block，添加 CALL 指令的特殊处理
     * 
     * ⚠️ 注意：这是 protected 方法，需要访问父类的私有成员
     * 实际实现需要在 .cpp 文件中完成
     */
    virtual BasicBlock enhanced_create_basic_block(
        uint64_t start_addr,
        std::unordered_map<uint64_t, std::vector<uint64_t>>& pending_edges
    );
};

} // namespace disassembly
