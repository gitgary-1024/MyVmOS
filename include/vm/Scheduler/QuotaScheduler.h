#ifndef LIVS_VM_SCHEDULER_QUOTA_SCHEDULER_H
#define LIVS_VM_SCHEDULER_QUOTA_SCHEDULER_H

#include "SchedulerBase.h"
#include <ext/pb_ds/assoc_container.hpp>
#include <ext/pb_ds/tree_policy.hpp>
#include <unordered_map>
#include <mutex>
#include <atomic>
#include <thread>
#include <chrono>
#include <iostream>

using namespace __gnu_pbds;

namespace vm {

/**
 * 名额制 + PBDS 优先队列调度器
 * 
 * 核心特性：
 * 1. 时间片轮转：每 10ms 分配一次运行名额
 * 2. PBDS 优先队列：O(log n) 的插入/删除性能
 * 3. 虚拟运行时间（vruntime）：保证公平性
 * 4. 优先级支持：高优先级 VM 优先调度
 */
class QuotaScheduler : public SchedulerBase {
public:
    QuotaScheduler();
    ~QuotaScheduler() override;
    
    // 调度器生命周期
    void start() override;
    void stop() override;
    bool is_running() const override { return scheduler_running_.load(std::memory_order_relaxed); }
    
    // VM管理
    void register_vm(uint64_t vm_id, std::weak_ptr<baseVM> vm) override;
    void unregister_vm(uint64_t vm_id) override;
    
    // 调度核心操作
    void schedule_vm(uint64_t vm_id) override;
    void block_vm(uint64_t vm_id, int reason) override;
    void wake_vm(uint64_t vm_id) override;
    
    // 状态查询
    int get_vm_priority(uint64_t vm_id) const override;
    uint64_t get_vm_vruntime(uint64_t vm_id) const override;
    bool has_quota(uint64_t vm_id) const override;
    
    // 动态配置
    void set_vm_priority(uint64_t vm_id, int priority) override;
    
    // 获取调度器名称
    std::string get_scheduler_name() const override { return "QuotaScheduler(PBDS)"; }

private:
    // 调度任务（PBDS 队列元素）
    struct ScheduleTask {
        uint64_t vm_id;
        int priority;           // 优先级（0=普通，1=高优，-1=低优）
        uint64_t vruntime;      // 虚拟运行时间
        
        // PBDS 比较算子：优先级高者优先，vruntime 小者优先
        bool operator<(const ScheduleTask& other) const {
            if (priority != other.priority) {
                return priority > other.priority;
            }
            return vruntime < other.vruntime;
        }
    };
    
    // PBDS 优先队列（自动平衡红黑树）
    typedef tree<
        ScheduleTask,
        null_type,
        std::less<ScheduleTask>,
        rb_tree_tag,
        tree_order_statistics_node_update
    > priority_queue_t;
    
    // 内部数据结构
    mutable std::mutex sched_mtx_;
    priority_queue_t ready_queue_;                      // PBDS 就绪队列
    std::unordered_map<uint64_t, ScheduleTask> vm_task_map_;  // VM→任务映射
    std::unordered_map<uint64_t, std::weak_ptr<baseVM>> vm_refs_; // VM 弱引用
    
    // 调度线程
    std::atomic<bool> scheduler_running_{false};
    std::thread scheduler_thread_;
    
    // 调度参数
    const int time_slice_ms_ = 10;               // 时间片（毫秒）
    const int max_instructions_per_slice_ = 50;  // 每个时间片最大指令数
    
    // 调度主循环
    void scheduler_loop();
    
    // 分配运行名额
    void allocate_quotas();
    
    // 更新虚拟运行时间
    void update_vruntime();
    
    // 辅助方法
    std::shared_ptr<baseVM> get_vm(uint64_t vm_id) const;
};

} // namespace vm

#endif // LIVS_VM_SCHEDULER_QUOTA_SCHEDULER_H
