#ifndef VM_SCHEDULER_HYBRID_SCHEDULER_H
#define VM_SCHEDULER_HYBRID_SCHEDULER_H

#include "vm/Scheduler/SchedulerBase.h"
#include "vm/baseVM.h"  // 需要完整类型
#include <set>
#include <queue>
#include <thread>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <ext/pb_ds/assoc_container.hpp>
#include <ext/pb_ds/tree_policy.hpp>

namespace vm {

/**
 * @brief CFS+EDF 混合调度实现
 * 
 * 核心思想：
 * 1. 实时 VM（高优先级）使用 EDF 调度 - 保证截止时间
 * 2. 普通 VM（低优先级）使用 CFS 调度 - 保证公平性
 * 3. 动态切换：VM 可以根据负载情况在 EDF/CFS 之间切换
 * 
 * 调度策略：
 * - 优先级 >= 1：EDF（实时任务）
 * - 优先级 == 0：CFS（普通任务）
 * - 优先级 <= -1：后台任务（最低优先级）
 * 
 * 特点：
 * - 兼顾实时性和公平性
 * - 适合混合负载场景
 * - Linux 内核 SCHED_OTHER + SCHED_DEADLINE 的简化版
 */
class HybridScheduler : public SchedulerBase {
public:
    HybridScheduler();
    ~HybridScheduler() override;

    // === 调度器生命周期 ===
    void start() override;
    void stop() override;
    bool is_running() const override;

    // === VM管理 ===
    void register_vm(uint64_t vm_id, std::weak_ptr<baseVM> vm) override;
    void unregister_vm(uint64_t vm_id) override;

    // === 调度核心操作 ===
    void schedule_vm(uint64_t vm_id) override;
    void block_vm(uint64_t vm_id, int reason) override;
    void wake_vm(uint64_t vm_id) override;

    // === 状态查询 ===
    int get_vm_priority(uint64_t vm_id) const override;
    uint64_t get_vm_vruntime(uint64_t vm_id) const override;
    bool has_quota(uint64_t vm_id) const override;

    // === 动态配置 ===
    void set_vm_priority(uint64_t vm_id, int priority) override;

    // === 获取调度器名称 ===
    std::string get_scheduler_name() const override;

protected:
    /**
     * @brief 调度线程主循环
     */
    void scheduler_loop();

    /**
     * @brief 选择下一个要运行的 VM
     * @return VM ID，如果没有可用 VM 返回 0
     */
    uint64_t pick_next_task();

    /**
     * @brief 从 EDF 队列选择（如果有实时任务）
     * @return VM ID，如果没有返回 0
     */
    uint64_t pick_edf_task();

    /**
     * @brief 从 CFS 队列选择（普通任务）
     * @return VM ID，如果没有返回 0
     */
    uint64_t pick_cfs_task();

    /**
     * @brief 更新所有 VM 的时间参数
     */
    void update_time_params();

    /**
     * @brief 获取 VM 实例
     */
    std::shared_ptr<baseVM> get_vm(uint64_t vm_id) const;

private:
    // CFS 任务结构（用于普通任务）
    struct CFSTask {
        uint64_t vm_id;
        uint64_t vruntime;      // 虚拟运行时间
        int weight;             // 权重
        
        bool operator<(const CFSTask& other) const {
            if (vruntime != other.vruntime) {
                return vruntime < other.vruntime;
            }
            return vm_id < other.vm_id;
        }
    };
    
    // EDF 任务结构（用于实时任务）
    struct EDFTask {
        uint64_t vm_id;
        uint64_t deadline_ns;   // 绝对截止时间
        uint64_t period_ns;     // 周期
        uint64_t vruntime;      // 虚拟运行时间
        int priority;
        
        bool operator<(const EDFTask& other) const {
            return deadline_ns > other.deadline_ns;
        }
    };
    
    // 核心数据结构
    mutable std::mutex sched_mtx_;           // 保护调度数据结构
    
    // EDF 队列（实时任务）
    std::priority_queue<EDFTask> edf_queue_;
    std::unordered_map<uint64_t, EDFTask> edf_task_map_;
    
    // CFS 队列（普通任务）
    std::set<CFSTask> cfs_queue_;
    std::unordered_map<uint64_t, CFSTask> cfs_task_map_;
    
    // VM 引用
    std::unordered_map<uint64_t, std::weak_ptr<baseVM>> vm_refs_;
    std::unordered_map<uint64_t, int> vm_priorities_;  // VM 优先级缓存
    
    // 调度控制
    std::atomic<bool> running_{false};
    std::thread scheduler_thread_;
    std::condition_variable cv_;             // 调度器条件变量
    std::atomic<int> time_slice_ms_{10};
    
    // 混合调度参数
    static constexpr int kRealTimePriority = 1;      // 实时任务优先级阈值
    static constexpr uint64_t kDefaultPeriodNs = 10000000;   // 10ms
    static constexpr uint64_t kDefaultDeadlineNs = 10000000; // 10ms
    static constexpr int kDefaultWeight = 1024;
    
    // 获取当前时间（纳秒）
    static uint64_t get_time_ns();
};

} // namespace vm

#endif // VM_SCHEDULER_HYBRID_SCHEDULER_H
