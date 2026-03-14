#ifndef LIVS_VM_SCHEDULER_MANAGER_H
#define LIVS_VM_SCHEDULER_MANAGER_H

#include "vm/Scheduler/SchedulerBase.h"
#include <memory>
#include <string>
#include <mutex>

// 前置声明
class baseVM;

/**
 * VM调度管理器 - 作为调度系统的统一入口
 * 采用策略模式，支持动态切换不同调度算法
 * 
 * 当前支持的算法：
 * - QuotaScheduler: 名额制 + PBDS 优先队列（默认）
 */
class SchedulerManager {
public:
    static SchedulerManager& instance();  // 单例
    
    // 初始化/销毁
    void start();
    void stop();
    bool is_running() const;
    
    // VM 注册/注销
    void register_vm(uint64_t vm_id, std::weak_ptr<baseVM> vm);
    void unregister_vm(uint64_t vm_id);
    
    // 调度核心接口（委托给具体调度算法）
    void schedule_vm(uint64_t vm_id);           // VM 请求调度
    void block_vm(uint64_t vm_id, int reason);  // VM 阻塞
    void wake_vm(uint64_t vm_id);               // VM 唤醒
    
    // 获取 VM 的调度上下文
    int get_vm_priority(uint64_t vm_id) const;
    uint64_t get_vm_vruntime(uint64_t vm_id) const;
    bool has_quota(uint64_t vm_id) const;
    
    // 设置优先级
    void set_vm_priority(uint64_t vm_id, int priority);
    
    // === 新增：动态切换调度算法 ===
    void set_scheduler(std::shared_ptr<vm::SchedulerBase> scheduler);
    std::string get_current_scheduler_name() const;

private:
    SchedulerManager();
    ~SchedulerManager();
    
    // 当前使用的调度器实例（策略模式）
    std::shared_ptr<vm::SchedulerBase> current_scheduler_;
    mutable std::mutex scheduler_switch_mtx_;  // 保护调度器切换
};

// 全局快捷函数
inline void scheduler_register_vm(uint64_t vm_id, std::weak_ptr<baseVM> vm) {
    SchedulerManager::instance().register_vm(vm_id, std::move(vm));
}

inline void scheduler_unregister_vm(uint64_t vm_id) {
    SchedulerManager::instance().unregister_vm(vm_id);
}

inline void scheduler_schedule_vm(uint64_t vm_id) {
    SchedulerManager::instance().schedule_vm(vm_id);
}

inline void scheduler_block_vm(uint64_t vm_id, int reason) {
    SchedulerManager::instance().block_vm(vm_id, reason);
}

inline void scheduler_wake_vm(uint64_t vm_id) {
    SchedulerManager::instance().wake_vm(vm_id);
}

#endif // LIVS_VM_SCHEDULER_MANAGER_H
