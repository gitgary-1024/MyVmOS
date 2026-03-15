#ifndef VM_SCHEDULER_CFS_SCHEDULER_H
#define VM_SCHEDULER_CFS_SCHEDULER_H

#include "vm/Scheduler/SchedulerBase.h"
#include "vm/baseVM.h"  // 需要完整类型
#include <set>
#include <thread>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <ext/pb_ds/assoc_container.hpp>
#include <ext/pb_ds/tree_policy.hpp>

namespace vm {

/**
 * @brief CFS（完全公平调度）实现
 * 
 * 核心思想：
 * 1. 总是选择 vruntime 最小的 VM 运行
 * 2. 保证所有 VM 长期来看获得公平的 CPU 时间
 * 3. 支持优先级加权（高优先级 VM vruntime 增长更慢）
 * 
 * 特点：
 * - 简单高效，O(log n) 性能
 * - 完全公平，无饥饿问题
 * - Linux 内核默认调度算法
 */
class CFSScheduler : public SchedulerBase {
public:
    CFSScheduler();
    ~CFSScheduler() override;

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
     * @brief 选择下一个要运行的 VM（vruntime 最小者）
     * @return VM ID，如果没有可用 VM 返回 0
     */
    uint64_t pick_next_task();

    /**
     * @brief 更新 VM 的 vruntime
     * @param vm_id VM ID
     */
    void update_vruntime(uint64_t vm_id);

    /**
     * @brief 获取 VM 实例
     */
    std::shared_ptr<baseVM> get_vm(uint64_t vm_id) const;

private:
    // CFS 运行队列（按 vruntime 排序）
    struct CFSTask {
        uint64_t vm_id;
        uint64_t vruntime;      // 虚拟运行时间
        int weight;             // 权重（基于优先级）
        
        // 比较算子：vruntime 小者优先
        bool operator<(const CFSTask& other) const {
            if (vruntime != other.vruntime) {
                return vruntime < other.vruntime;
            }
            return vm_id < other.vm_id;  // vruntime 相同时按 ID 排序
        }
    };
    
    using cfs_queue_t = std::set<CFSTask>;
    
    // 核心数据结构
    cfs_queue_t run_queue_;                  // CFS 运行队列
    std::unordered_map<uint64_t, CFSTask> vm_task_map_;  // VM -> Task 映射
    std::unordered_map<uint64_t, std::weak_ptr<baseVM>> vm_refs_;  // VM 引用
    
    // 调度控制
    std::atomic<bool> running_{false};       // 调度器运行状态
    std::thread scheduler_thread_;           // 调度线程
    mutable std::mutex sched_mtx_;           // 保护调度数据结构（复用）
    std::condition_variable cv_;             // 调度器条件变量
    std::atomic<int> time_slice_ms_{10};     // 时间片（毫秒）
    
    // CFS 参数
    static constexpr int64_t kMinVruntime = 0;        // 最小 vruntime
    static constexpr int64_t kMaxVruntime = INT64_MAX; // 最大 vruntime
    static constexpr int kDefaultWeight = 1024;       // 默认权重
    
    // 权重表（优先级 -> 权重）
    static const std::unordered_map<int, int> priority_to_weight_;
};

} // namespace vm

#endif // VM_SCHEDULER_CFS_SCHEDULER_H
