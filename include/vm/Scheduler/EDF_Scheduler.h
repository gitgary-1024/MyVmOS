#ifndef VM_SCHEDULER_EDF_SCHEDULER_H
#define VM_SCHEDULER_EDF_SCHEDULER_H

#include "vm/Scheduler/SchedulerBase.h"
#include "vm/baseVM.h"  // 需要完整类型
#include <queue>
#include <thread>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <functional>

namespace vm {

/**
 * @brief EDF（最早截止时间优先）调度实现
 * 
 * 核心思想：
 * 1. 总是选择截止时间最早的 VM 运行
 * 2. 适合实时任务和延迟敏感型应用
 * 3. 理论上可以达到 100% CPU 利用率
 * 
 * 特点：
 * - 实时性最好
 * - 需要任务提供截止时间信息
 * - 可能出现"多米诺效应"（一个任务延迟导致连锁反应）
 * 
 * 注意：
 * 本实现结合了 CFS 的 vruntime 机制来防止饥饿
 */
class EDFScheduler : public SchedulerBase {
public:
    EDFScheduler();
    ~EDFScheduler() override;

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

    // === EDF 特有接口 ===
    /**
     * @brief 设置 VM 的截止时间（纳秒）
     * @param vm_id VM ID
     * @param deadline_ns 截止时间（相对当前时间的纳秒数）
     */
    void set_vm_deadline(uint64_t vm_id, uint64_t deadline_ns);

    /**
     * @brief 获取 VM 的剩余截止时间
     * @param vm_id VM ID
     * @return 剩余纳秒数，如果 VM 不存在返回 0
     */
    uint64_t get_remaining_deadline(uint64_t vm_id) const;

protected:
    /**
     * @brief 调度线程主循环
     */
    void scheduler_loop();

    /**
     * @brief 选择下一个要运行的 VM（截止时间最早者）
     * @return VM ID，如果没有可用 VM 返回 0
     */
    uint64_t pick_next_task();

    /**
     * @brief 更新所有 VM 的截止时间
     */
    void update_deadlines();

    /**
     * @brief 获取 VM 实例
     */
    std::shared_ptr<baseVM> get_vm(uint64_t vm_id) const;

private:
    // EDF 任务结构
    struct EDFTask {
        uint64_t vm_id;
        uint64_t deadline_ns;     // 绝对截止时间（纳秒）
        uint64_t period_ns;       // 周期（纳秒）
        uint64_t vruntime;        // 虚拟运行时间（用于公平性）
        int priority;             // 优先级
        
        // 比较算子：截止时间早者优先
        bool operator<(const EDFTask& other) const {
            return deadline_ns > other.deadline_ns;  // priority_queue 是大顶堆，所以用>
        }
    };
    
    // 核心数据结构
    std::priority_queue<EDFTask> edf_queue_; // EDF 优先队列
    std::unordered_map<uint64_t, EDFTask> vm_task_map_;  // VM -> Task 映射
    std::unordered_map<uint64_t, std::weak_ptr<baseVM>> vm_refs_;  // VM 引用
    
    // 调度控制
    std::atomic<bool> running_{false};       // 调度器运行状态
    std::thread scheduler_thread_;           // 调度线程
    mutable std::mutex sched_mtx_;           // 保护调度数据结构（复用）
    std::condition_variable cv_;             // 调度器条件变量
    std::atomic<int> time_slice_ms_{10};     // 时间片（毫秒）
    
    // EDF 参数
    static constexpr uint64_t kDefaultPeriodNs = 10000000;   // 默认周期 10ms（纳秒）
    static constexpr uint64_t kDefaultDeadlineNs = 10000000; // 默认截止时间 10ms
    
    // 获取当前时间（纳秒）
    static uint64_t get_time_ns();
};

} // namespace vm

#endif // VM_SCHEDULER_EDF_SCHEDULER_H
