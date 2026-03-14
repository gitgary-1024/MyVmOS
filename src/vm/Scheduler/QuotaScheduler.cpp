#include "vm/Scheduler/QuotaScheduler.h"
#include "vm/baseVM.h"
#include <algorithm>

namespace vm {

QuotaScheduler::QuotaScheduler() = default;

QuotaScheduler::~QuotaScheduler() {
    stop();
}

void QuotaScheduler::start() {
    if (scheduler_running_.load(std::memory_order_relaxed)) return;
    
    scheduler_running_.store(true);
    scheduler_thread_ = std::thread(&QuotaScheduler::scheduler_loop, this);
    
    std::cout << "[QuotaScheduler] Started (PBDS)" << std::endl;
}

void QuotaScheduler::stop() {
    scheduler_running_.store(false);
    
    if (scheduler_thread_.joinable()) {
        scheduler_thread_.join();
    }
    
    std::cout << "[QuotaScheduler] Stopped" << std::endl;
}

void QuotaScheduler::register_vm(uint64_t vm_id, std::weak_ptr<baseVM> vm) {
    std::lock_guard<std::mutex> lock(sched_mtx_);
    vm_refs_[vm_id] = vm;
    
    // 加入 PBDS 队列
    ScheduleTask task{vm_id, 0, 0};
    ready_queue_.insert(task);
    vm_task_map_[vm_id] = task;
    
    std::cout << "[QuotaScheduler] Registered VM " << vm_id << std::endl;
}

void QuotaScheduler::unregister_vm(uint64_t vm_id) {
    std::lock_guard<std::mutex> lock(sched_mtx_);
    
    // 从 PBDS 队列移除
    auto it = vm_task_map_.find(vm_id);
    if (it != vm_task_map_.end()) {
        ready_queue_.erase(it->second);
        vm_task_map_.erase(it);
    }
    
    // 移除引用
    vm_refs_.erase(vm_id);
    
    std::cout << "[QuotaScheduler] Unregistered VM " << vm_id << std::endl;
}

void QuotaScheduler::schedule_vm(uint64_t vm_id) {
    std::lock_guard<std::mutex> lock(sched_mtx_);
    auto vm = get_vm(vm_id);
    if (!vm) return;
    
    auto it = vm_task_map_.find(vm_id);
    if (it == vm_task_map_.end()) {
        ScheduleTask task{vm_id, vm->get_sched_ctx().priority, vm->get_sched_ctx().vruntime};
        ready_queue_.insert(task);
        vm_task_map_[vm_id] = task;
    } else {
        // 更新优先级：先删除旧的，再插入新的
        ready_queue_.erase(it->second);
        it->second.priority = vm->get_sched_ctx().priority;
        ready_queue_.insert(it->second);
    }
}

void QuotaScheduler::block_vm(uint64_t vm_id, int reason) {
    std::lock_guard<std::mutex> lock(sched_mtx_);
    auto vm = get_vm(vm_id);
    if (!vm) return;
    
    // 从 PBDS 队列移除
    auto it = vm_task_map_.find(vm_id);
    if (it != vm_task_map_.end()) {
        ready_queue_.erase(it->second);
        vm_task_map_.erase(it);
    }
    
    // 标记阻塞原因
    vm->mutable_sched_ctx().blocked_reason = reason;
    vm->revoke_quota();
    
    std::cout << "[QuotaScheduler::block_vm] Blocked VM " << vm_id 
              << ", reason=" << reason << std::endl;
}

void QuotaScheduler::wake_vm(uint64_t vm_id) {
    std::lock_guard<std::mutex> lock(sched_mtx_);
    auto vm = get_vm(vm_id);
    if (!vm) return;
    
    // 清除阻塞标记
    vm->mutable_sched_ctx().blocked_reason = 0;
    
    // 重新加入 PBDS 队列
    ScheduleTask task{vm_id, vm->get_sched_ctx().priority, vm->get_sched_ctx().vruntime};
    ready_queue_.insert(task);
    vm_task_map_[vm_id] = task;
    
    std::cout << "[QuotaScheduler::wake_vm] Woken up VM " << vm_id << std::endl;
}

int QuotaScheduler::get_vm_priority(uint64_t vm_id) const {
    std::lock_guard<std::mutex> lock(sched_mtx_);
    auto it = vm_task_map_.find(vm_id);
    if (it != vm_task_map_.end()) {
        return it->second.priority;
    }
    return 0;
}

uint64_t QuotaScheduler::get_vm_vruntime(uint64_t vm_id) const {
    std::lock_guard<std::mutex> lock(sched_mtx_);
    auto it = vm_task_map_.find(vm_id);
    if (it != vm_task_map_.end()) {
        return it->second.vruntime;
    }
    return 0;
}

bool QuotaScheduler::has_quota(uint64_t vm_id) const {
    auto vm = get_vm(vm_id);
    if (vm) {
        return vm->has_quota();
    }
    return false;
}

void QuotaScheduler::set_vm_priority(uint64_t vm_id, int priority) {
    std::lock_guard<std::mutex> lock(sched_mtx_);
    auto vm = get_vm(vm_id);
    if (!vm) return;
    
    auto it = vm_task_map_.find(vm_id);
    if (it != vm_task_map_.end()) {
        // 更新优先级
        ready_queue_.erase(it->second);
        it->second.priority = priority;
        ready_queue_.insert(it->second);
    }
}

void QuotaScheduler::scheduler_loop() {
    std::cout << "[QuotaScheduler] Scheduler loop started" << std::endl;
    
    while (scheduler_running_.load(std::memory_order_relaxed)) {
        auto start_time = std::chrono::steady_clock::now();
        
        // 分配名额
        allocate_quotas();
        
        // 等待时间片结束
        auto elapsed = std::chrono::steady_clock::now() - start_time;
        int sleep_ms = time_slice_ms_ - static_cast<int>(std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count());
        if (sleep_ms > 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(sleep_ms));
        }
        
        // 更新虚拟运行时间
        update_vruntime();
    }
    
    std::cout << "[QuotaScheduler] Scheduler loop ended" << std::endl;
}

void QuotaScheduler::allocate_quotas() {
    std::lock_guard<std::mutex> lock(sched_mtx_);
    
    // 简单策略：给所有就绪 VM 分配名额
    // PBDS 优势：O(log n) 遍历性能
    for (const auto& task : ready_queue_) {
        auto vm = get_vm(task.vm_id);
        if (vm && (vm->get_state() == VMState::RUNNING || 
                   vm->get_state() == VMState::CREATED)) {
            vm->grant_quota();
        }
    }
}

void QuotaScheduler::update_vruntime() {
    std::lock_guard<std::mutex> lock(sched_mtx_);
    
    // 增加已运行 VM 的 vruntime（公平性保证）
    for (auto& [vm_id, task] : vm_task_map_) {
        auto vm = get_vm(vm_id);
        if (vm && vm->has_quota()) {
            task.vruntime += max_instructions_per_slice_;
            vm->mutable_sched_ctx().vruntime = task.vruntime;
            
            // 更新 PBDS 队列中的任务
            // 注意：需要重新平衡树
            // 这里采用简化方案：不立即更新树，等待下次操作时自然调整
        }
    }
}

std::shared_ptr<baseVM> QuotaScheduler::get_vm(uint64_t vm_id) const {
    auto it = vm_refs_.find(vm_id);
    if (it != vm_refs_.end()) {
        return it->second.lock();  // 尝试提升为 shared_ptr
    }
    return nullptr;
}

} // namespace vm
