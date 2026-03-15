#include "vm/SchedulerManager.h"
#include "vm/Scheduler/QuotaScheduler.h"
#include <iostream>

// 静态实例
SchedulerManager& SchedulerManager::instance() {
    static SchedulerManager manager;
    return manager;
}

SchedulerManager::SchedulerManager() {
    // 默认使用名额制 + PBDS 调度器
    current_scheduler_ = std::make_shared<vm::QuotaScheduler>();
}

SchedulerManager::~SchedulerManager() {
    stop();
}

void SchedulerManager::start() {
    if (current_scheduler_) {
        current_scheduler_->start();
        std::cout << "[SchedulerManager] Using scheduler: " 
                  << get_current_scheduler_name() << std::endl;
    }
}

void SchedulerManager::stop() {
    if (current_scheduler_) {
        current_scheduler_->stop();
    }
}

bool SchedulerManager::is_running() const {
    return current_scheduler_ && current_scheduler_->is_running();
}

void SchedulerManager::register_vm(uint64_t vm_id, std::weak_ptr<baseVM> vm) {
    std::lock_guard<std::mutex> lock(scheduler_switch_mtx_);
    if (current_scheduler_) {
        current_scheduler_->register_vm(vm_id, std::move(vm));
    }
}

void SchedulerManager::unregister_vm(uint64_t vm_id) {
    std::lock_guard<std::mutex> lock(scheduler_switch_mtx_);
    if (current_scheduler_) {
        current_scheduler_->unregister_vm(vm_id);
    }
}

void SchedulerManager::schedule_vm(uint64_t vm_id) {
    if (current_scheduler_) {
        current_scheduler_->schedule_vm(vm_id);
    }
}

void SchedulerManager::block_vm(uint64_t vm_id, int reason) {
    if (current_scheduler_) {
        current_scheduler_->block_vm(vm_id, reason);
    }
}

void SchedulerManager::wake_vm(uint64_t vm_id) {
    if (current_scheduler_) {
        current_scheduler_->wake_vm(vm_id);
    }
}

int SchedulerManager::get_vm_priority(uint64_t vm_id) const {
    std::lock_guard<std::mutex> lock(scheduler_switch_mtx_);
    if (current_scheduler_) {
        return current_scheduler_->get_vm_priority(vm_id);
    }
    return 0;
}

uint64_t SchedulerManager::get_vm_vruntime(uint64_t vm_id) const {
    std::lock_guard<std::mutex> lock(scheduler_switch_mtx_);
    if (current_scheduler_) {
        return current_scheduler_->get_vm_vruntime(vm_id);
    }
    return 0;
}

bool SchedulerManager::has_quota(uint64_t vm_id) const {
    std::lock_guard<std::mutex> lock(scheduler_switch_mtx_);
    if (current_scheduler_) {
        return current_scheduler_->has_quota(vm_id);
    }
    return false;
}

void SchedulerManager::set_vm_priority(uint64_t vm_id, int priority) {
    std::lock_guard<std::mutex> lock(scheduler_switch_mtx_);
    if (current_scheduler_) {
        current_scheduler_->set_vm_priority(vm_id, priority);
    }
}

void SchedulerManager::set_scheduler(std::shared_ptr<vm::SchedulerBase> scheduler) {
    std::lock_guard<std::mutex> lock(scheduler_switch_mtx_);
    
    // 停止当前调度器
    if (current_scheduler_) {
        current_scheduler_->stop();
    }
    
    // 切换新调度器并分配唯一 ID
    current_scheduler_ = scheduler;
    
    if (current_scheduler_) {
        // 分配唯一的调度器 ID
        current_scheduler_->scheduler_id_ = next_scheduler_id_.fetch_add(1);
        std::cout << "[SchedulerManager] Assigned scheduler ID: " 
                  << current_scheduler_->get_scheduler_id() << std::endl;
        
        // 启动新调度器
        current_scheduler_->start();
        std::cout << "[SchedulerManager] Switched to scheduler: " 
                  << get_current_scheduler_name() << std::endl;
    }
}

std::string SchedulerManager::get_current_scheduler_name() const {
    std::lock_guard<std::mutex> lock(scheduler_switch_mtx_);
    if (current_scheduler_) {
        return current_scheduler_->get_scheduler_name();
    }
    return "None";
}

void SchedulerManager::stop_scheduler(uint64_t scheduler_id) {
    std::lock_guard<std::mutex> lock(scheduler_switch_mtx_);
    
    if (current_scheduler_ && current_scheduler_->get_scheduler_id() == scheduler_id) {
        std::cout << "[SchedulerManager] Stopping scheduler ID: " << scheduler_id << std::endl;
        // 直接调用 stop() 方法，使用 condition_variable 立即唤醒
        current_scheduler_->stop();
    } else {
        std::cout << "[SchedulerManager] No matching scheduler found for ID: " << scheduler_id << std::endl;
    }
}
