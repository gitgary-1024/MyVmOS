#include "disassembly/x86/X86ExecutorRegistry.h"
#include <iostream>

namespace disassembly {
namespace x86 {

// ===== 单例实例 =====
X86ExecutorRegistry& X86ExecutorRegistry::instance() {
    static X86ExecutorRegistry registry;
    return registry;
}

// ===== 注册执行器工厂 =====
void X86ExecutorRegistry::register_executor(const std::string& mnemonic, 
                                            ExecutorFactory factory) {
    executors_[mnemonic] = std::move(factory);
}

// ===== 为指令创建执行器 =====
ExecutorVariant X86ExecutorRegistry::create_executor(const std::string& mnemonic) {
    auto it = executors_.find(mnemonic);
    if (it != executors_.end()) {
        return it->second();
    }
    
    // 未知指令，返回默认的 HLT 执行器（安全 fallback）
    #ifdef DEBUG_EXECUTION
    std::cerr << "[WARNING] Unknown instruction: " << mnemonic 
              << ", using HLT as fallback" << std::endl;
    #endif
    
    return HltExecutor{};
}

// ===== 初始化所有执行器 =====
void X86ExecutorRegistry::initialize_all_executors() {
    // 数据传输指令
    register_executor("mov", make_mov_executor());
    register_executor("push", make_push_executor());
    register_executor("pop", make_pop_executor());
    
    // 算术运算指令
    register_executor("add", make_add_executor());
    register_executor("sub", make_sub_executor());
    
    // 逻辑运算指令
    // TODO: 添加 and, or, xor, not, test 等
    
    // 控制流指令
    register_executor("jmp", make_jmp_executor());
    register_executor("call", make_call_executor());
    register_executor("ret", make_ret_executor());
    
    // 比较指令
    register_executor("cmp", make_cmp_executor());
    register_executor("test", make_test_executor());
    
    // 系统指令
    register_executor("hlt", make_hlt_executor());
    
    #ifdef DEBUG_EXECUTION
    std::cout << "[INFO] X86ExecutorRegistry initialized with " 
              << executors_.size() << " executors" << std::endl;
    #endif
}

// ===== MOV 执行器工厂 =====
X86ExecutorRegistry::ExecutorFactory X86ExecutorRegistry::make_mov_executor() {
    return []() -> ExecutorVariant {
        return MovExecutor{};
    };
}

// ===== ADD 执行器工厂 =====
X86ExecutorRegistry::ExecutorFactory X86ExecutorRegistry::make_add_executor() {
    return []() -> ExecutorVariant {
        return AddExecutor{};
    };
}

// ===== SUB 执行器工厂 =====
X86ExecutorRegistry::ExecutorFactory X86ExecutorRegistry::make_sub_executor() {
    return []() -> ExecutorVariant {
        return SubExecutor{};
    };
}

// ===== HLT 执行器工厂 =====
X86ExecutorRegistry::ExecutorFactory X86ExecutorRegistry::make_hlt_executor() {
    return []() -> ExecutorVariant {
        return HltExecutor{};
    };
}

// ===== JMP 执行器工厂 =====
X86ExecutorRegistry::ExecutorFactory X86ExecutorRegistry::make_jmp_executor() {
    return []() -> ExecutorVariant {
        return JmpExecutor{};
    };
}

// ===== CALL 执行器工厂 =====
X86ExecutorRegistry::ExecutorFactory X86ExecutorRegistry::make_call_executor() {
    return []() -> ExecutorVariant {
        return CallExecutor{};
    };
}

// ===== RET 执行器工厂 =====
X86ExecutorRegistry::ExecutorFactory X86ExecutorRegistry::make_ret_executor() {
    return []() -> ExecutorVariant {
        return RetExecutor{};
    };
}

// ===== PUSH 执行器工厂 =====
X86ExecutorRegistry::ExecutorFactory X86ExecutorRegistry::make_push_executor() {
    return []() -> ExecutorVariant {
        return PushExecutor{};
    };
}

// ===== POP 执行器工厂 =====
X86ExecutorRegistry::ExecutorFactory X86ExecutorRegistry::make_pop_executor() {
    return []() -> ExecutorVariant {
        return PopExecutor{};
    };
}

// ===== CMP 执行器工厂 =====
X86ExecutorRegistry::ExecutorFactory X86ExecutorRegistry::make_cmp_executor() {
    return []() -> ExecutorVariant {
        return CmpExecutor{};
    };
}

// ===== TEST 执行器工厂 =====
X86ExecutorRegistry::ExecutorFactory X86ExecutorRegistry::make_test_executor() {
    return []() -> ExecutorVariant {
        return TestExecutor{};
    };
}

} // namespace x86
} // namespace disassembly
