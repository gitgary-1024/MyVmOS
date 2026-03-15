#ifndef LIVS_VM_SCHEDULER_BASE_H
#define LIVS_VM_SCHEDULER_BASE_H

#include <cstdint>
#include <memory>
#include <vector>
#include <string>
#include <atomic>

// 前置声明
class baseVM;

namespace vm {

/**
 * 调度器基类 - 所有调度算法的接口
 * 采用策略模式，支持动态切换不同调度算法
 */
class SchedulerBase {
public:
    // 唯一调度器 ID（由 SchedulerManager 分配）
    uint64_t scheduler_id_;


    SchedulerBase();
    virtual ~SchedulerBase() = default;
    
    // 获取调度器唯一 ID
    uint64_t get_scheduler_id() const { return scheduler_id_; }
    
    // 调度器生命周期
    virtual void start() = 0;      // 启动调度器
    virtual void stop() = 0;       // 停止调度器
    virtual bool is_running() const = 0;
    
    // VM 管理
    virtual void register_vm(uint64_t vm_id, std::weak_ptr<baseVM> vm) = 0;
    virtual void unregister_vm(uint64_t vm_id) = 0;
    
    // 调度核心操作
    virtual void schedule_vm(uint64_t vm_id) = 0;           // 加入就绪队列
    virtual void block_vm(uint64_t vm_id, int reason) = 0;  // 阻塞（让出名额）
    virtual void wake_vm(uint64_t vm_id) = 0;               // 唤醒（重新入队）
    
    // 状态查询
    virtual int get_vm_priority(uint64_t vm_id) const = 0;
    virtual uint64_t get_vm_vruntime(uint64_t vm_id) const = 0;
    virtual bool has_quota(uint64_t vm_id) const = 0;
    
    // 动态配置
    virtual void set_vm_priority(uint64_t vm_id, int priority) = 0;
    
    // 获取调度器名称（用于日志和调试）
    virtual std::string get_scheduler_name() const = 0;
    
protected:
};

} // namespace vm

#endif // LIVS_VM_SCHEDULER_BASE_H
