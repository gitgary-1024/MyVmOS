#include "vm/VmManager.h"
#include "vm/baseVM.h"
#include "vm/SchedulerManager.h"
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
    
    // 注册到调度器（直接访问，无需通过对象池）
    SchedulerManager::instance().register_vm(vm->get_vm_id(), std::weak_ptr<baseVM>(vm));
}

void VmManager::unregister_vm(uint64_t vm_id) {
    std::lock_guard<std::mutex> lock(vm_mtx_);
    auto it = vms_.find(vm_id);
    if (it != vms_.end()) {
        vms_.erase(it);
        std::cout << "[VmManager] Unregistered VM " << vm_id << std::endl;
    }
    
    // 从调度器注销
    SchedulerManager::instance().unregister_vm(vm_id);
    
    // 清理相关的待处理中断
    std::lock_guard<std::mutex> int_lock(interrupt_mtx_);
    auto pending_it = pending_interrupts_.find(vm_id);
    if (pending_it != pending_interrupts_.end()) {
        // 取消 TimeoutManager 的超时定时器
        if (pending_it->second.timeout_id != 0) {
            TimeoutManager::instance().cancel_timeout(pending_it->second.timeout_id);
        }
        pending_interrupts_.erase(pending_it);
    }
}

void VmManager::destroy_vm(uint64_t vm_id) {
    // 异步销毁 VM，避免阻塞主线程
    vm_lifecycle_pool_.submit([this, vm_id]() {
        std::cout << "[VmManager] Destroying VM " << vm_id << " asynchronously" << std::endl;
        
        // 先从管理器注销
        unregister_vm(vm_id);
        
        // shared_ptr 会自动释放内存
        std::cout << "[VmManager] VM " << vm_id << " destroyed and memory released" << std::endl;
    });
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
    
    // 启动调度器
    SchedulerManager::instance().start();
    
    // 订阅 VM 的中断请求
    route_subscribe(MessageType::INTERRUPT_REQUEST, [this](const Message& msg) {
        this->handle_vm_interrupt_request(msg);
    });
    
    // 订阅外设的中断结果
    route_subscribe(MessageType::INTERRUPT_RESULT_READY, [this](const Message& msg) {
        this->handle_interrupt_result(msg);
    });
    
    std::cout << "[VmManager] Started (with scheduler)" << std::endl;
}

void VmManager::stop() {
    running_.store(false, std::memory_order_relaxed);
    worker_cv_.notify_all();
    
    if (worker_thread_.joinable()) {
        worker_thread_.join();
    }
    
    // 停止调度器
    SchedulerManager::instance().stop();
    
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

VmManager::InterruptStats VmManager::get_interrupt_stats() const {
    InterruptStats stats;
    stats.total_requests = interrupt_stats_.total_requests.load();
    stats.completed_requests = interrupt_stats_.completed_requests.load();
    stats.timeout_requests = interrupt_stats_.timeout_requests.load();
    
    {
        std::lock_guard<std::mutex> lock(interrupt_mtx_);
        stats.pending_requests = pending_interrupts_.size();
    }
    
    if (stats.completed_requests > 0) {
        stats.avg_latency_ms = static_cast<double>(interrupt_stats_.total_latency_ms.load()) / 
                               static_cast<double>(stats.completed_requests);
    }
    
    return stats;
}

void VmManager::worker_loop() {
    std::cout << "[VmManager] Worker loop started" << std::endl;
    
    while (running_.load(std::memory_order_relaxed)) {
        std::unique_lock<std::mutex> lock(worker_mtx_);
        
        // 1. 处理异步中断结果（优先级最高）
        {
            std::lock_guard<std::mutex> async_lock(async_result_mtx_);
            while (!async_result_queue_.empty()) {
                AsyncInterruptResult async_result = async_result_queue_.front();
                async_result_queue_.pop();
                lock.unlock();
                
                process_async_result(async_result);
                
                lock.lock();
            }
        }
        
        // 2. 处理 TimeoutManager 的超时回调
        TimeoutManager::instance().process_timeouts();
        
        // 3. 检查是否有超时的中断请求（备用机制，以防 TimeoutManager 失效）
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
        
        // 4. 处理超时的中断
        for (const auto& pending : timed_out) {
            process_interrupt_timeout(pending);
        }
        
        // 5. 动态计算等待时间（使用 TimeoutManager）
        int64_t next_timeout = TimeoutManager::instance().get_next_timeout_ms();
        auto wait_duration = std::chrono::milliseconds(50);  // 默认 50ms
        
        if (next_timeout == -1) {
            // 没有超时，使用默认等待时间
            wait_duration = std::chrono::milliseconds(50);
        } else if (next_timeout == 0) {
            // 已经超时，立即继续
            continue;
        } else {
            // 有超时，取最小值
            wait_duration = std::chrono::milliseconds(
                std::min(static_cast<int64_t>(50), next_timeout)
            );
        }
        
        // 6. 等待一段时间或被唤醒
        worker_cv_.wait_for(lock, wait_duration, 
                           [this] { return !running_.load(std::memory_order_relaxed); });
    }
    
    std::cout << "[VmManager] Worker loop ended" << std::endl;
}

void VmManager::forward_interrupt_to_router(const Message& msg) {
    // 将中断请求转发到主路由树 - 修改 receiver 为外设管理器
    std::cout << "[VmManager] Forwarding interrupt request to router" << std::endl;
    
    // 创建新消息，修改 sender 和 receiver
    Message forwarded_msg = msg;
    forwarded_msg.sender_id = MODULE_VM_MANAGER;  // 标记为 VM Manager 转发的
    forwarded_msg.receiver_id = MODULE_PERIPH_MANAGER;  // 发送到外设管理器
    
    route_send(forwarded_msg);
}

void VmManager::process_interrupt_timeout(const PendingInterrupt& pending) {
    std::cout << "[VmManager] Interrupt timeout for VM " << pending.vm_id 
              << ", periph " << pending.periph_id 
              << ", priority=" << static_cast<int>(pending.priority) << std::endl;
              
    // 发送超时结果给 VM
    InterruptResult result{pending.vm_id, -1, true, pending.interrupt_type, pending.priority};
    result.completion_time = std::chrono::steady_clock::now();
    Message timeout_msg(MODULE_VM_MANAGER, pending.vm_id, MessageType::INTERRUPT_RESULT_READY);
    timeout_msg.set_payload(result);
    
    // ✅ 唤醒 VM（即使是超时也要唤醒，让 VM 处理错误）- 委托给 SchedulerManager
    SchedulerManager::instance().wake_vm(pending.vm_id);
    
    // 调用 VM 的中断处理
    auto vm = get_vm(pending.vm_id);
    if (vm) {
        vm->handle_interrupt(result);
    } else {
        std::cerr << "[VmManager] VM " << pending.vm_id << " not found for timeout" << std::endl;
    }
    
    // 更新统计
    interrupt_stats_.timeout_requests++;
}

void VmManager::process_async_result(AsyncInterruptResult& async_result) {
    std::cout << "[VmManager] Processing async interrupt result for VM " << async_result.vm_id
              << ": " << async_result.result.return_value << std::endl;
    
    // 清理待处理中断记录
    {
        std::lock_guard<std::mutex> lock(interrupt_mtx_);
        pending_interrupts_.erase(async_result.vm_id);
    }
    
    // ✅ 唤醒 VM（重新分配名额）- 委托给 SchedulerManager
    SchedulerManager::instance().wake_vm(async_result.vm_id);
    
    // 将结果转发给对应的 VM
    auto vm = get_vm(async_result.vm_id);
    if (vm) {
        vm->handle_interrupt(async_result.result);
        
        // 计算延迟并更新统计
        if (!async_result.result.is_timeout && 
            async_result.request_time != std::chrono::steady_clock::time_point{}) {
            int64_t latency = async_result.result.get_total_latency_ms(async_result.request_time);
            interrupt_stats_.total_latency_ms += latency;
            interrupt_stats_.completed_requests++;
            
            std::cout << "[VmManager] Interrupt completed for VM " << async_result.vm_id
                      << ", latency=" << latency << "ms" << std::endl;
        }
    } else {
        std::cerr << "[VmManager] VM " << async_result.vm_id 
                  << " not found for result delivery" << std::endl;
    }
}

void VmManager::on_interrupt_request(const Message& msg) {
    // 忽略自己转发的消息（避免死循环）
    if (msg.sender_id == MODULE_VM_MANAGER) {
        return;  // 这是 VM Manager 转发的消息，不是 VM 直接发送的
    }
    
    auto* req = msg.get_payload<InterruptRequest>();
    if (!req) return;
    
    std::cout << "[VmManager] Processing interrupt request from VM " << req->vm_id 
              << " to periph " << req->periph_id 
              << ", type=" << static_cast<int>(req->interrupt_type)
              << ", priority=" << static_cast<int>(req->priority) << std::endl;
    
    // 验证 VM 是否存在
    {
        std::lock_guard<std::mutex> lock(vm_mtx_);
        if (vms_.find(req->vm_id) == vms_.end()) {
            std::cerr << "[VmManager] VM " << req->vm_id << " not registered" << std::endl;
            return;
        }
    }
    
    // ✅ 阻塞 VM（让出调度名额）- 委托给 SchedulerManager
    SchedulerManager::instance().block_vm(req->vm_id, 1); // 1=等待外设
    
    // 记录待处理的中断（加入优先级队列）
    PendingInterrupt pending{
        req->vm_id,
        req->periph_id,
        req->interrupt_type,
        req->priority,
        req->timeout_ms,
        std::chrono::steady_clock::now(),
        std::chrono::steady_clock::now()  // enqueue_time
    };
    
    // 使用 TimeoutManager 注册超时回调
    pending.timeout_id = TimeoutManager::instance().register_timeout(
        req->timeout_ms,
        [this, vm_id = req->vm_id]() {
            // 超时回调：发送超时错误给 VM
            std::cout << "[VmManager] Timeout callback triggered for VM " << vm_id << std::endl;
            
            InterruptResult result{vm_id, -1, true, InterruptType::SYSTEM, InterruptPriority::NORMAL};
            result.completion_time = std::chrono::steady_clock::now();
            
            Message timeout_msg(MODULE_VM_MANAGER, vm_id, MessageType::INTERRUPT_RESULT_READY);
            timeout_msg.set_payload(result);
            
            // 转发结果给 VM
            handle_interrupt_result(timeout_msg);
        },
        "Interrupt_VM" + std::to_string(req->vm_id)
    );
    
    {
        std::lock_guard<std::mutex> lock(interrupt_mtx_);
        pending_interrupts_[req->vm_id] = pending;
        interrupt_priority_queue_.push(pending);  // 加入优先级队列
    }
    
    // 更新统计
    interrupt_stats_.total_requests++;
    
    // 转发到主路由树
    forward_interrupt_to_router(msg);
}

void VmManager::on_interrupt_result(const Message& msg) {
    auto* result = msg.get_payload<InterruptResult>();
    if (!result) return;
    
    std::cout << "[VmManager] Processing interrupt result for VM " << result->vm_id 
              << ": " << result->return_value 
              << ", priority=" << static_cast<int>(result->priority) << std::endl;
    
    // 获取请求时间以计算延迟
    std::chrono::steady_clock::time_point request_time;
    TimeoutManager::TimeoutId timeout_id = 0;
    {
        std::lock_guard<std::mutex> lock(interrupt_mtx_);
        auto it = pending_interrupts_.find(result->vm_id);
        if (it != pending_interrupts_.end()) {
            request_time = it->second.request_time;
            timeout_id = it->second.timeout_id;  // 获取超时 ID
        }
    }
    
    // 取消超时定时器（因为已经收到结果）
    if (timeout_id != 0) {
        TimeoutManager::instance().cancel_timeout(timeout_id);
    }
    
    // 创建结果副本并设置完成时间戳
    InterruptResult result_copy = *result;
    result_copy.completion_time = std::chrono::steady_clock::now();
    
    // 将结果加入异步处理队列（稍后由工作线程处理）
    {
        std::lock_guard<std::mutex> lock(async_result_mtx_);
        async_result_queue_.push({
            result->vm_id,
            result_copy,
            request_time
        });
    }
    
    // 唤醒工作线程立即处理
    worker_cv_.notify_all();
}