#ifndef LIVS_VM_DISASSEMBLY_X86_EXECUTOR_REGISTRY_H
#define LIVS_VM_DISASSEMBLY_X86_EXECUTOR_REGISTRY_H

#include "X86Executors.h"
#include "../IR.h"
#include <unordered_map>
#include <functional>
#include <string>
#include <memory>

namespace disassembly {
namespace x86 {

// ===== 执行器注册表（单例模式）=====
class X86ExecutorRegistry {
public:
    // 执行器工厂函数类型
    using ExecutorFactory = std::function<ExecutorVariant()>;
    
private:
    // 助记符 -> 工厂函数的映射表
    std::unordered_map<std::string, ExecutorFactory> executors_;
    
    // 私有构造函数（单例模式）
    X86ExecutorRegistry() = default;
    
public:
    // 禁止拷贝和赋值
    X86ExecutorRegistry(const X86ExecutorRegistry&) = delete;
    X86ExecutorRegistry& operator=(const X86ExecutorRegistry&) = delete;
    
    // 获取单例实例
    static X86ExecutorRegistry& instance();
    
    // 注册执行器工厂
    void register_executor(const std::string& mnemonic, ExecutorFactory factory);
    
    // 为指令创建执行器
    ExecutorVariant create_executor(const std::string& mnemonic);
    
    // 初始化所有 x86 指令的执行器
    void initialize_all_executors();
    
private:
    // 各指令的执行器工厂创建函数
    static ExecutorFactory make_mov_executor();
    static ExecutorFactory make_add_executor();
    static ExecutorFactory make_sub_executor();
    static ExecutorFactory make_hlt_executor();
    static ExecutorFactory make_jmp_executor();
    static ExecutorFactory make_call_executor();
    static ExecutorFactory make_ret_executor();
    static ExecutorFactory make_push_executor();
    static ExecutorFactory make_pop_executor();
    static ExecutorFactory make_cmp_executor();
    static ExecutorFactory make_test_executor();
};

} // namespace x86
} // namespace disassembly

#endif // LIVS_VM_DISASSEMBLY_X86_EXECUTOR_REGISTRY_H
