#include "vm/VmManager.h"
#include "vm/baseVM.h"
#include "router/RouterCore.h"
#include <iostream>
#include <chrono>
#include <algorithm>

// 静态实例
VmManager& VmManager::instance() {
    static VmManager manager;
    return manager;
}

VmManager::~VmManager() {
    stop();
}

void VmManager::register_vm(std::shared_ptr<baseVM> vm) {
    if (!vm) return;
    
    std::lock_guard<std::mutex> lock(vm_mtx_);
    vms_[vm->get_vm_id()] = vm;  // 不 move，保持引用计数
    std::cout << "[VmManager] Registered VM " << vm->get_vm_id() << std::endl;
}

void VmManager::unregister_vm(uint64_t vm_id) {
    std::lock_guard<std::mutex> lock(vm_mtx_);
    auto it = vms_.find(vm_id);
    if (it != vms_.end()) {
        vms_.erase(it);
        std::cout << "[VmManager] Unregistered VM " << vm_id << std::endl;
    }
    
    // 清理相关的待处理中断
    std::lock_guard<std::mutex> int_lock(interrupt_mtx_);
    auto pending_it = pending_interrupts_.find(vm_id);
    if (pending_it != pending_interrupts_.end()) {
        pending_interrupts_.erase(pending_it);
    }
}

std::shared_ptr<baseVM> VmManager::get_vm(uint64_t vm_id) {
    std::lock_guard<std::mutex> lock(vm_mtx_);
    auto it = vms_.find(vm_id);
    return (it != vms_.end()) ? it->second : nullptr;
}

void VmManager::handle_vm_interrupt_request(const Message& msg) {
    std::cout << "[VmManager] Handle VM interrupt request: type=" << static_cast<int>(msg.type)
              << ", sender=" << msg.sender_id << std::endl;
              
    if (msg.type == MessageType::INTERRUPT_REQUEST && msg.is_payload<InterruptRequest>()) {
        on_interrupt_request(msg);
    } else {
        std::cerr << "[VmManager] Invalid interrupt request message" << std::endl;
    }
}

void VmManager::handle_interrupt_result(const Message& msg) {
    std::cout << "[VmManager] Handle interrupt result: type=" << static_cast<int>(msg.type)
              << ", sender=" << msg.sender_id << std::endl;
              
    if (msg.type == MessageType::INTERRUPT_RESULT_READY && msg.is_payload<InterruptResult>()) {
        on_interrupt_result(msg);
    } else {
        std::cerr << "[VmManager] Invalid interrupt result message" << std::endl;
    }
}

void VmManager::start() {
    if (running_.load(std::memory_order_relaxed)) return;
    
    running_.store(true, std::memory_order_relaxed);
    worker_thread_ = std::thread(&VmManager::worker_loop, this);
    
    // 连接到 RouterCore - 作为外部接收器
    auto& router = RouterCore::instance();
    router.connect_external_receiver([this](const Message& msg) {
        // 处理来自 Router 的消息（主要是中断结果）
        if (msg.type == MessageType::INTERRUPT_RESULT_READY) {
            handle_interrupt_result(msg);
        }
    });
    
    std::cout << "[VmManager] Started" << std::endl;
}

void VmManager::stop() {
    running_.store(false, std::memory_order_relaxed);
    worker_cv_.notify_all();
    
    if (worker_thread_.joinable()) {
        worker_thread_.join();
    }
    
    std::cout << "[VmManager] Stopped" << std::endl;
}

size_t VmManager::get_registered_vm_count() const {
    std::lock_guard<std::mutex> lock(vm_mtx_);
    return vms_.size();
}

std::vector<uint64_t> VmManager::get_all_vm_ids() const {
    std::lock_guard<std::mutex> lock(vm_mtx_);
    std::vector<uint64_t> ids;
    ids.reserve(vms_.size());
    
    for (const auto& pair : vms_) {
        ids.push_back(pair.first);
    }
    
    return ids;
}

void VmManager::worker_loop() {
    std::cout << "[VmManager] Worker loop started" << std::endl;
    
    while (running_.load(std::memory_order_relaxed)) {
        std::unique_lock<std::mutex> lock(worker_mtx_);
        
        // 检查是否有超时的中断请求
        std::vector<PendingInterrupt> timed_out;
        {
            std::lock_guard<std::mutex> int_lock(interrupt_mtx_);
            auto now = std::chrono::steady_clock::now();
            
            for (auto it = pending_interrupts_.begin(); it != pending_interrupts_.end();) {
                const auto& pending = it->second;
                auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                    now - pending.request_time);
                
                if (elapsed.count() >= pending.timeout_ms) {
                    timed_out.push_back(pending);
                    it = pending_interrupts_.erase(it);
                } else {
                    ++it;
                }
            }
        }
        
        // 处理超时的中断
        for (const auto& pending : timed_out) {
            process_interrupt_timeout(pending);
        }
        
        // 等待一段时间或被唤醒
        worker_cv_.wait_for(lock, std::chrono::milliseconds(100), 
                           [this] { return !running_.load(std::memory_order_relaxed); });
    }
    
    std::cout << "[VmManager] Worker loop ended" << std::endl;
}

void VmManager::forward_interrupt_to_router(const Message& msg) {
    // 将中断请求转发到主路由树
    std::cout << "[VmManager] Forwarding interrupt request to router" << std::endl;
    route_send(msg);
}

void VmManager::process_interrupt_timeout(const PendingInterrupt& pending) {
    std::cout << "[VmManager] Interrupt timeout for VM " << pending.vm_id 
              << ", periph " << pending.periph_id << std::endl;
              
    // 发送超时结果给VM
    InterruptResult result{pending.vm_id, -1, true}; // timeout = true
    Message timeout_msg(MODULE_INTERRUPT_SCHEDULER, pending.vm_id, MessageType::INTERRUPT_RESULT_READY);
    timeout_msg.set_payload(result);
    
    route_send(timeout_msg);
}

void VmManager::on_interrupt_request(const Message& msg) {
    auto* req = msg.get_payload<InterruptRequest>();
    if (!req) return;
    
    std::cout << "[VmManager] Processing interrupt request from VM " << req->vm_id 
              << " to periph " << req->periph_id << std::endl;
    
    // 验证VM是否存在
    {
        std::lock_guard<std::mutex> lock(vm_mtx_);
        if (vms_.find(req->vm_id) == vms_.end()) {
            std::cerr << "[VmManager] VM " << req->vm_id << " not registered" << std::endl;
            return;
        }
    }
    
    // 记录待处理的中断
    PendingInterrupt pending{
        req->vm_id,
        req->periph_id,
        req->interrupt_type,
        req->timeout_ms,
        std::chrono::steady_clock::now()
    };
    
    {
        std::lock_guard<std::mutex> lock(interrupt_mtx_);
        pending_interrupts_[req->vm_id] = pending;
    }
    
    // 转发到主路由树
    forward_interrupt_to_router(msg);
}

void VmManager::on_interrupt_result(const Message& msg) {
    auto* result = msg.get_payload<InterruptResult>();
    if (!result) return;
    
    std::cout << "[VmManager] Processing interrupt result for VM " << result->vm_id 
              << ": " << result->return_value << std::endl;
    
    // 清理待处理中断记录
    {
        std::lock_guard<std::mutex> lock(interrupt_mtx_);
        pending_interrupts_.erase(result->vm_id);
    }
    
    // 将结果转发给对应的VM
    auto vm = get_vm(result->vm_id);
    if (vm) {
        vm->handle_interrupt(*result);
    } else {
        std::cerr << "[VmManager] VM " << result->vm_id << " not found for result delivery" << std::endl;
    }
}