#include "vm/Scheduler/Hybrid_Scheduler.h"
#include <iostream>
#include <algorithm>

namespace vm {

HybridScheduler::HybridScheduler() {}

HybridScheduler::~HybridScheduler() {
    stop();
}

void HybridScheduler::start() {
    if (running_.exchange(true)) {
        return;
    }
    
    scheduler_thread_ = std::thread(&HybridScheduler::scheduler_loop, this);
    std::cout << "[Hybrid] Scheduler loop started" << std::endl;
}

void HybridScheduler::stop() {
    if (!running_.exchange(false)) {
        return;  // 已经在停止状态
    }
    
    // 唤醒等待中的调度线程
    cv_.notify_one();
    
    if (scheduler_thread_.joinable()) {
        scheduler_thread_.join();
    }
    
    std::cout << "[Hybrid] Scheduler stopped" << std::endl;
}

bool HybridScheduler::is_running() const {
    return running_.load();
}

void HybridScheduler::register_vm(uint64_t vm_id, std::weak_ptr<baseVM> vm) {
    std::lock_guard<std::mutex> lock(sched_mtx_);
    
    vm_refs_[vm_id] = vm;
    vm_priorities_[vm_id] = 0;  // 默认普通优先级
    
    // 根据优先级决定加入哪个队列
    int priority = 0;  // 初始为 0
    uint64_t now = get_time_ns();
    
    if (priority >= kRealTimePriority) {
        // EDF 队列（实时任务）
        EDFTask edf_task{
            .vm_id = vm_id,
            .deadline_ns = now + kDefaultDeadlineNs,
            .period_ns = kDefaultPeriodNs,
            .vruntime = 0,
            .priority = priority
        };
        
        edf_task_map_[vm_id] = edf_task;
        edf_queue_.push(edf_task);
        
        std::cout << "[Hybrid] Registered VM " << vm_id << " to EDF queue" << std::endl;
    } else {
        // CFS 队列（普通任务）
        CFSTask cfs_task{
            .vm_id = vm_id,
            .vruntime = 0,
            .weight = kDefaultWeight
        };
        
        cfs_task_map_[vm_id] = cfs_task;
        cfs_queue_.insert(cfs_task);
        
        std::cout << "[Hybrid] Registered VM " << vm_id << " to CFS queue" << std::endl;
    }
}

void HybridScheduler::unregister_vm(uint64_t vm_id) {
    std::lock_guard<std::mutex> lock(sched_mtx_);
    
    // 从 EDF 队列删除
    auto edf_it = edf_task_map_.find(vm_id);
    if (edf_it != edf_task_map_.end()) {
        edf_task_map_.erase(edf_it);
        // 重建队列
        std::priority_queue<EDFTask> new_queue;
        while (!edf_queue_.empty()) {
            auto task = edf_queue_.top();
            edf_queue_.pop();
            if (task.vm_id != vm_id) {
                new_queue.push(task);
            }
        }
        edf_queue_ = std::move(new_queue);
    }
    
    // 从 CFS 队列删除
    auto cfs_it = cfs_task_map_.find(vm_id);
    if (cfs_it != cfs_task_map_.end()) {
        cfs_queue_.erase(cfs_it->second);
        cfs_task_map_.erase(cfs_it);
    }
    
    vm_refs_.erase(vm_id);
    vm_priorities_.erase(vm_id);
    
    std::cout << "[Hybrid] Unregistered VM " << vm_id << std::endl;
}

void HybridScheduler::schedule_vm(uint64_t vm_id) {
    std::lock_guard<std::mutex> lock(sched_mtx_);
    
    auto prio_it = vm_priorities_.find(vm_id);
    if (prio_it == vm_priorities_.end()) {
        return;
    }
    
    if (prio_it->second >= kRealTimePriority) {
        // EDF 任务
        auto it = edf_task_map_.find(vm_id);
        if (it != edf_task_map_.end()) {
            uint64_t now = get_time_ns();
            it->second.deadline_ns = now + it->second.period_ns;
            edf_queue_.push(it->second);
        }
    } else {
        // CFS 任务
        auto it = cfs_task_map_.find(vm_id);
        if (it != cfs_task_map_.end()) {
            // 更新 vruntime
            int64_t delta = time_slice_ms_ * 100;
            int64_t scaled_delta = (delta * kDefaultWeight) / it->second.weight;
            
            // 从运行队列删除旧任务
            cfs_queue_.erase(it->second);
            
            // 更新 vruntime
            it->second.vruntime += scaled_delta;
            
            // 重新插入到运行队列（按新的 vruntime 排序）
            cfs_queue_.insert(it->second);
        }
    }
}

void HybridScheduler::block_vm(uint64_t vm_id, int reason) {
    std::lock_guard<std::mutex> lock(sched_mtx_);
    
    auto prio_it = vm_priorities_.find(vm_id);
    if (prio_it == vm_priorities_.end()) {
        return;
    }
    
    if (prio_it->second >= kRealTimePriority) {
        // EDF：保持截止时间
        auto it = edf_task_map_.find(vm_id);
        if (it != edf_task_map_.end()) {
            // 不更新截止时间
        }
    } else {
        // CFS：从队列移除
        auto it = cfs_task_map_.find(vm_id);
        if (it != cfs_task_map_.end()) {
            cfs_queue_.erase(it->second);
        }
    }
    
    (void)reason;
}

void HybridScheduler::wake_vm(uint64_t vm_id) {
    std::lock_guard<std::mutex> lock(sched_mtx_);
    
    auto prio_it = vm_priorities_.find(vm_id);
    if (prio_it == vm_priorities_.end()) {
        return;
    }
    
    if (prio_it->second >= kRealTimePriority) {
        // EDF：检查截止时间
        auto it = edf_task_map_.find(vm_id);
        if (it != edf_task_map_.end()) {
            uint64_t now = get_time_ns();
            if (it->second.deadline_ns <= now) {
                it->second.deadline_ns = now + it->second.period_ns;
            }
            edf_queue_.push(it->second);
        }
    } else {
        // CFS：重新加入队列
        auto it = cfs_task_map_.find(vm_id);
        if (it != cfs_task_map_.end()) {
            cfs_queue_.insert(it->second);
        }
    }
}

int HybridScheduler::get_vm_priority(uint64_t vm_id) const {
    std::lock_guard<std::mutex> lock(sched_mtx_);
    
    auto it = vm_priorities_.find(vm_id);
    if (it == vm_priorities_.end()) {
        return 0;
    }
    
    return it->second;
}

uint64_t HybridScheduler::get_vm_vruntime(uint64_t vm_id) const {
    std::lock_guard<std::mutex> lock(sched_mtx_);
    
    // 先查 EDF
    auto edf_it = edf_task_map_.find(vm_id);
    if (edf_it != edf_task_map_.end()) {
        return edf_it->second.vruntime;
    }
    
    // 再查 CFS
    auto cfs_it = cfs_task_map_.find(vm_id);
    if (cfs_it != cfs_task_map_.end()) {
        return cfs_it->second.vruntime;
    }
    
    return 0;
}

bool HybridScheduler::has_quota(uint64_t vm_id) const {
    std::lock_guard<std::mutex> lock(sched_mtx_);
    
    auto prio_it = vm_priorities_.find(vm_id);
    if (prio_it == vm_priorities_.end()) {
        return false;
    }
    
    if (prio_it->second >= kRealTimePriority) {
        return edf_task_map_.find(vm_id) != edf_task_map_.end();
    } else {
        return cfs_task_map_.find(vm_id) != cfs_task_map_.end();
    }
}

void HybridScheduler::set_vm_priority(uint64_t vm_id, int priority) {
    std::lock_guard<std::mutex> lock(sched_mtx_);
    
    auto old_prio_it = vm_priorities_.find(vm_id);
    if (old_prio_it == vm_priorities_.end()) {
        return;
    }
    
    int old_priority = old_prio_it->second;
    bool was_edf = (old_priority >= kRealTimePriority);
    bool is_edf = (priority >= kRealTimePriority);
    
    vm_priorities_[vm_id] = priority;
    
    // 如果调度策略改变，需要从一个队列移到另一个队列
    if (was_edf && !is_edf) {
        // EDF -> CFS
        auto it = edf_task_map_.find(vm_id);
        if (it != edf_task_map_.end()) {
            // 创建 CFS 任务
            CFSTask cfs_task{
                .vm_id = vm_id,
                .vruntime = it->second.vruntime,
                .weight = kDefaultWeight
            };
            
            cfs_task_map_[vm_id] = cfs_task;
            cfs_queue_.insert(cfs_task);
            
            // 删除 EDF 任务
            edf_task_map_.erase(it);
            
            std::cout << "[Hybrid] VM " << vm_id << " switched from EDF to CFS" << std::endl;
        }
    } else if (!was_edf && is_edf) {
        // CFS -> EDF
        auto it = cfs_task_map_.find(vm_id);
        if (it != cfs_task_map_.end()) {
            // 创建 EDF 任务
            uint64_t now = get_time_ns();
            EDFTask edf_task{
                .vm_id = vm_id,
                .deadline_ns = now + kDefaultDeadlineNs,
                .period_ns = kDefaultPeriodNs,
                .vruntime = it->second.vruntime,
                .priority = priority
            };
            
            edf_task_map_[vm_id] = edf_task;
            edf_queue_.push(edf_task);
            
            // 删除 CFS 任务
            cfs_queue_.erase(it->second);
            cfs_task_map_.erase(it);
            
            std::cout << "[Hybrid] VM " << vm_id << " switched from CFS to EDF" << std::endl;
        }
    }
}

std::string HybridScheduler::get_scheduler_name() const {
    return "Hybrid(CFS+EDF)";
}

void HybridScheduler::scheduler_loop() {
    std::cout << "[Hybrid] Scheduler loop started (scheduler_id=" << scheduler_id_ << ")" << std::endl;
    
    while (running_) {
        auto start_time = std::chrono::steady_clock::now();
        
        // 选择下一个 VM
        uint64_t next_vm = pick_next_task();
        if (next_vm != 0) {
            // 更新时间参数（模拟执行）
            schedule_vm(next_vm);
            std::cout << "[Hybrid] Scheduled VM " << next_vm << std::endl;
        }
        
        // 更新时间参数
        update_time_params();
        
        // 等待下一个时间片，使用 condition_variable 实现高效等待
        auto elapsed = std::chrono::steady_clock::now() - start_time;
        int remaining_ms = time_slice_ms_ - static_cast<int>(
            std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count()
        );
        
        if (remaining_ms > 0) {
            std::unique_lock<std::mutex> lock(sched_mtx_);
            // 使用 condition_variable 等待，支持立即唤醒
            cv_.wait_for(lock, std::chrono::milliseconds(remaining_ms));
        }
    }
    
    std::cout << "[Hybrid] Scheduler loop exited" << std::endl;
}

uint64_t HybridScheduler::pick_next_task() {
    std::lock_guard<std::mutex> lock(sched_mtx_);
    
    // 优先选择 EDF 任务（实时任务）
    uint64_t edf_vm = pick_edf_task();
    if (edf_vm != 0) {
        return edf_vm;
    }
    
    // 没有 EDF 任务时，选择 CFS 任务
    return pick_cfs_task();
}

uint64_t HybridScheduler::pick_edf_task() {
    if (edf_queue_.empty()) {
        return 0;
    }
    
    return edf_queue_.top().vm_id;
}

uint64_t HybridScheduler::pick_cfs_task() {
    if (cfs_queue_.empty()) {
        return 0;
    }
    
    return cfs_queue_.begin()->vm_id;
}

void HybridScheduler::update_time_params() {
    // 混合调度不需要特别处理
}

uint64_t HybridScheduler::get_time_ns() {
    auto now = std::chrono::steady_clock::now();
    auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
        now.time_since_epoch()
    ).count();
    return static_cast<uint64_t>(ns);
}

} // namespace vm
