#include "vm/Scheduler/EDF_Scheduler.h"
#include <iostream>
#include <algorithm>

namespace vm {

EDFScheduler::EDFScheduler() {}

EDFScheduler::~EDFScheduler() {
    stop();
}

void EDFScheduler::start() {
    if (running_.exchange(true)) {
        return;
    }
    
    scheduler_thread_ = std::thread(&EDFScheduler::scheduler_loop, this);
    std::cout << "[EDF] Scheduler loop started" << std::endl;
}

void EDFScheduler::stop() {
    if (!running_.exchange(false)) {
        return;  // 已经在停止状态
    }
    
    // 唤醒等待中的调度线程
    cv_.notify_one();
    
    if (scheduler_thread_.joinable()) {
        scheduler_thread_.join();
    }
    
    std::cout << "[EDF] Scheduler stopped" << std::endl;
}

bool EDFScheduler::is_running() const {
    return running_.load();
}

void EDFScheduler::register_vm(uint64_t vm_id, std::weak_ptr<baseVM> vm) {
    std::lock_guard<std::mutex> lock(sched_mtx_);
    
    vm_refs_[vm_id] = vm;
    
    // 初始化 EDF 任务
    uint64_t now = get_time_ns();
    EDFTask task{
        .vm_id = vm_id,
        .deadline_ns = now + kDefaultDeadlineNs,
        .period_ns = kDefaultPeriodNs,
        .vruntime = 0,
        .priority = 0
    };
    
    vm_task_map_[vm_id] = task;
    edf_queue_.push(task);
    
    std::cout << "[EDF] Registered VM " << vm_id 
              << " with deadline " << task.deadline_ns << "ns" << std::endl;
}

void EDFScheduler::unregister_vm(uint64_t vm_id) {
    std::lock_guard<std::mutex> lock(sched_mtx_);
    
    vm_task_map_.erase(vm_id);
    vm_refs_.erase(vm_id);
    
    // 注意：priority_queue 不支持直接删除，需要重建队列
    std::priority_queue<EDFTask> new_queue;
    while (!edf_queue_.empty()) {
        auto task = edf_queue_.top();
        edf_queue_.pop();
        if (task.vm_id != vm_id) {
            new_queue.push(task);
        }
    }
    edf_queue_ = std::move(new_queue);
    
    std::cout << "[EDF] Unregistered VM " << vm_id << std::endl;
}

void EDFScheduler::schedule_vm(uint64_t vm_id) {
    std::lock_guard<std::mutex> lock(sched_mtx_);
    
    auto it = vm_task_map_.find(vm_id);
    if (it == vm_task_map_.end()) {
        return;
    }
    
    // EDF: 更新截止时间
    uint64_t now = get_time_ns();
    it->second.deadline_ns = now + it->second.period_ns;
    
    // 重新加入队列
    edf_queue_.push(it->second);
}

void EDFScheduler::block_vm(uint64_t vm_id, int reason) {
    std::lock_guard<std::mutex> lock(sched_mtx_);
    
    auto it = vm_task_map_.find(vm_id);
    if (it == vm_task_map_.end()) {
        return;
    }
    
    // EDF: 阻塞时不更新截止时间（保持原有截止时间）
    // 这样唤醒后如果截止时间已过，会被立即调度
    
    (void)reason;
}

void EDFScheduler::wake_vm(uint64_t vm_id) {
    std::lock_guard<std::mutex> lock(sched_mtx_);
    
    auto it = vm_task_map_.find(vm_id);
    if (it == vm_task_map_.end()) {
        return;
    }
    
    // 如果截止时间已过，立即重新设置
    uint64_t now = get_time_ns();
    if (it->second.deadline_ns <= now) {
        it->second.deadline_ns = now + it->second.period_ns;
    }
    
    edf_queue_.push(it->second);
}

int EDFScheduler::get_vm_priority(uint64_t vm_id) const {
    std::lock_guard<std::mutex> lock(sched_mtx_);
    
    auto it = vm_task_map_.find(vm_id);
    if (it == vm_task_map_.end()) {
        return 0;
    }
    
    return it->second.priority;
}

uint64_t EDFScheduler::get_vm_vruntime(uint64_t vm_id) const {
    std::lock_guard<std::mutex> lock(sched_mtx_);
    
    auto it = vm_task_map_.find(vm_id);
    if (it == vm_task_map_.end()) {
        return 0;
    }
    
    return it->second.vruntime;
}

bool EDFScheduler::has_quota(uint64_t vm_id) const {
    std::lock_guard<std::mutex> lock(sched_mtx_);
    return vm_task_map_.find(vm_id) != vm_task_map_.end();
}

void EDFScheduler::set_vm_priority(uint64_t vm_id, int priority) {
    std::lock_guard<std::mutex> lock(sched_mtx_);
    
    auto it = vm_task_map_.find(vm_id);
    if (it == vm_task_map_.end()) {
        return;
    }
    
    it->second.priority = priority;
}

std::string EDFScheduler::get_scheduler_name() const {
    return "EDF";
}

void EDFScheduler::set_vm_deadline(uint64_t vm_id, uint64_t deadline_ns) {
    std::lock_guard<std::mutex> lock(sched_mtx_);
    
    auto it = vm_task_map_.find(vm_id);
    if (it == vm_task_map_.end()) {
        return;
    }
    
    uint64_t now = get_time_ns();
    it->second.deadline_ns = now + deadline_ns;
    it->second.period_ns = deadline_ns;
}

uint64_t EDFScheduler::get_remaining_deadline(uint64_t vm_id) const {
    std::lock_guard<std::mutex> lock(sched_mtx_);
    
    auto it = vm_task_map_.find(vm_id);
    if (it == vm_task_map_.end()) {
        return 0;
    }
    
    uint64_t now = get_time_ns();
    if (it->second.deadline_ns <= now) {
        return 0;  // 已过期
    }
    
    return it->second.deadline_ns - now;
}

void EDFScheduler::scheduler_loop() {
    std::cout << "[EDF] Scheduler loop started (scheduler_id=" << scheduler_id_ << ")" << std::endl;
    
    while (running_) {
        auto start_time = std::chrono::steady_clock::now();
        
        // 选择下一个 VM
        uint64_t next_vm = pick_next_task();
        if (next_vm != 0) {
            // 更新截止时间（模拟执行）
            schedule_vm(next_vm);
            std::cout << "[EDF] Scheduled VM " << next_vm 
                      << " with deadline=" << get_remaining_deadline(next_vm) << "ns" << std::endl;
        }
        
        // 更新截止时间
        update_deadlines();
        
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
    
    std::cout << "[EDF] Scheduler loop exited" << std::endl;
}

uint64_t EDFScheduler::pick_next_task() {
    std::lock_guard<std::mutex> lock(sched_mtx_);
    
    if (edf_queue_.empty()) {
        return 0;
    }
    
    // 选择截止时间最早的 VM
    EDFTask task = edf_queue_.top();
    return task.vm_id;
}

void EDFScheduler::update_deadlines() {
    // EDF 不需要特别处理，截止时间在 schedule_vm 时更新
}

uint64_t EDFScheduler::get_time_ns() {
    auto now = std::chrono::steady_clock::now();
    auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
        now.time_since_epoch()
    ).count();
    return static_cast<uint64_t>(ns);
}

} // namespace vm
