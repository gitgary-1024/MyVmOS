#include "vm/Scheduler/CFS_Scheduler.h"
#include <iostream>
#include <algorithm>

namespace vm {

// 初始化优先级到权重的映射表
const std::unordered_map<int, int> CFSScheduler::priority_to_weight_ = {
    {-2, 512},    // 最低优先级
    {-1, 820},    // 低优先级
    {0, 1024},    // 普通优先级
    {1, 1280},    // 高优先级
    {2, 2048}     // 最高优先级
};

CFSScheduler::CFSScheduler() {}

CFSScheduler::~CFSScheduler() {
    stop();
}

void CFSScheduler::start() {
    if (running_.exchange(true)) {
        return;  // 已经在运行
    }
    
    scheduler_thread_ = std::thread(&CFSScheduler::scheduler_loop, this);
    std::cout << "[CFS] Scheduler loop started" << std::endl;
}

void CFSScheduler::stop() {
    if (!running_.exchange(false)) {
        return;  // 已经在停止状态
    }
    
    // 唤醒等待中的调度线程
    cv_.notify_one();
    
    if (scheduler_thread_.joinable()) {
        scheduler_thread_.join();
    }
    
    std::cout << "[CFS] Scheduler stopped" << std::endl;
}

bool CFSScheduler::is_running() const {
    return running_.load();
}

void CFSScheduler::register_vm(uint64_t vm_id, std::weak_ptr<baseVM> vm) {
    std::lock_guard<std::mutex> lock(sched_mtx_);
    
    vm_refs_[vm_id] = vm;
    
    // 初始化 CFS 任务
    CFSTask task{
        .vm_id = vm_id,
        .vruntime = kMinVruntime,
        .weight = kDefaultWeight
    };
    
    vm_task_map_[vm_id] = task;
    run_queue_.insert(task);
    
    std::cout << "[CFS] Registered VM " << vm_id << " with weight " << task.weight << std::endl;
}

void CFSScheduler::unregister_vm(uint64_t vm_id) {
    std::lock_guard<std::mutex> lock(sched_mtx_);
    
    auto it = vm_task_map_.find(vm_id);
    if (it != vm_task_map_.end()) {
        run_queue_.erase(it->second);
        vm_task_map_.erase(it);
    }
    
    vm_refs_.erase(vm_id);
    
    std::cout << "[CFS] Unregistered VM " << vm_id << std::endl;
}

void CFSScheduler::schedule_vm(uint64_t vm_id) {
    std::lock_guard<std::mutex> lock(sched_mtx_);
    
    auto it = vm_task_map_.find(vm_id);
    if (it == vm_task_map_.end()) {
        return;
    }
    
    // 更新 vruntime 并重新插入队列
    update_vruntime(vm_id);
}

void CFSScheduler::block_vm(uint64_t vm_id, int reason) {
    std::lock_guard<std::mutex> lock(sched_mtx_);
    
    auto it = vm_task_map_.find(vm_id);
    if (it == vm_task_map_.end()) {
        return;
    }
    
    // 从运行队列移除（但不删除 vruntime 信息）
    run_queue_.erase(it->second);
    
    (void)reason;  //  unused
}

void CFSScheduler::wake_vm(uint64_t vm_id) {
    std::lock_guard<std::mutex> lock(sched_mtx_);
    
    auto it = vm_task_map_.find(vm_id);
    if (it == vm_task_map_.end()) {
        return;
    }
    
    // 重新加入运行队列（保持原有 vruntime）
    run_queue_.insert(it->second);
}

int CFSScheduler::get_vm_priority(uint64_t vm_id) const {
    std::lock_guard<std::mutex> lock(sched_mtx_);
    
    auto it = vm_task_map_.find(vm_id);
    if (it == vm_task_map_.end()) {
        return 0;
    }
    
    // 根据权重反推优先级
    for (const auto& [prio, weight] : priority_to_weight_) {
        if (weight == it->second.weight) {
            return prio;
        }
    }
    
    return 0;  // 默认优先级
}

uint64_t CFSScheduler::get_vm_vruntime(uint64_t vm_id) const {
    std::lock_guard<std::mutex> lock(sched_mtx_);
    
    auto it = vm_task_map_.find(vm_id);
    if (it == vm_task_map_.end()) {
        return 0;
    }
    
    return it->second.vruntime;
}

bool CFSScheduler::has_quota(uint64_t vm_id) const {
    // CFS 总是有运行名额（只要有 VM 在运行队列中）
    std::lock_guard<std::mutex> lock(sched_mtx_);
    return run_queue_.find(CFSTask{vm_id, 0, 0}) != run_queue_.end();
}

void CFSScheduler::set_vm_priority(uint64_t vm_id, int priority) {
    std::lock_guard<std::mutex> lock(sched_mtx_);
    
    auto it = vm_task_map_.find(vm_id);
    if (it == vm_task_map_.end()) {
        return;
    }
    
    // 查找对应权重
    auto wit = priority_to_weight_.find(priority);
    int new_weight = (wit != priority_to_weight_.end()) ? wit->second : kDefaultWeight;
    
    // 更新任务
    CFSTask old_task = it->second;
    run_queue_.erase(old_task);
    
    it->second.weight = new_weight;
    run_queue_.insert(it->second);
}

std::string CFSScheduler::get_scheduler_name() const {
    return "CFS";
}

void CFSScheduler::scheduler_loop() {
    std::cout << "[CFS] Scheduler loop started (scheduler_id=" << scheduler_id_ << ")" << std::endl;
    
    while (running_) {
        auto start_time = std::chrono::steady_clock::now();
        
        // 选择下一个 VM 并更新其 vruntime
        uint64_t next_vm = pick_next_task();
        if (next_vm != 0) {
            // 更新被选中 VM 的 vruntime（模拟执行）
            update_vruntime(next_vm);
            std::cout << "[CFS] Scheduled VM " << next_vm 
                      << " with vruntime=" << get_vm_vruntime(next_vm) << std::endl;
        }
        
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
    
    std::cout << "[CFS] Scheduler loop exited" << std::endl;
}

uint64_t CFSScheduler::pick_next_task() {
    std::lock_guard<std::mutex> lock(sched_mtx_);
    
    if (run_queue_.empty()) {
        return 0;
    }
    
    // 选择 vruntime 最小的 VM
    auto it = run_queue_.begin();
    return it->vm_id;
}

void CFSScheduler::update_vruntime(uint64_t vm_id) {
    auto it = vm_task_map_.find(vm_id);
    if (it == vm_task_map_.end()) {
        return;
    }
    
    // CFS 核心：根据权重调整 vruntime 增长速度
    // 权重越高，vruntime 增长越慢，获得 CPU 时间越多
    int64_t delta = time_slice_ms_ * 100;  // 基础增长量
    int64_t scaled_delta = (delta * kDefaultWeight) / it->second.weight;
    
    // 从运行队列删除旧任务
    run_queue_.erase(it->second);
    
    // 更新 vruntime
    it->second.vruntime += scaled_delta;
    
    // 确保 vruntime 不会超过范围
    if (it->second.vruntime > kMaxVruntime) {
        it->second.vruntime = kMaxVruntime;
    }
    
    // 重新插入到运行队列（按新的 vruntime 排序）
    run_queue_.insert(it->second);
    
    // 更新 VM 的 sched_ctx
    auto vm = get_vm(vm_id);
    if (vm) {
        vm->mutable_sched_ctx().vruntime = it->second.vruntime;
    }
}

std::shared_ptr<baseVM> CFSScheduler::get_vm(uint64_t vm_id) const {
    auto it = vm_refs_.find(vm_id);
    if (it != vm_refs_.end()) {
        return it->second.lock();
    }
    return nullptr;
}

} // namespace vm
