#ifndef LIVS_VM_MANAGER_H
#define LIVS_VM_MANAGER_H

#include <unordered_map>
#include <vector>
#include <mutex>
#include <memory>
#include <atomic>
#include <thread>
#include <condition_variable>
#include <queue>
#include <chrono>
#include "../router/MessageProtocol.h"
#include "../utils/ThreadPool.h"
#include "../utils/TimeoutManager.h"  // 新增：统一超时管理

// 前置声明
class baseVM;

// VM管理器：作为所有 VM 的统一中断入口点
class VmManager {
public:
    static VmManager& instance();
    
    // VM 生命周期管理
    void register_vm(std::shared_ptr<baseVM> vm);
    void unregister_vm(uint64_t vm_id);
    std::shared_ptr<baseVM> get_vm(uint64_t vm_id);
    
    // VM 创建（普通方式，保持兼容性）
    template<typename VMType, typename... Args>
    static std::shared_ptr<VMType> make_vm(Args&&... args) {
        return std::make_shared<VMType>(std::forward<Args>(args)...);
    }
    
    // VM 销毁（异步优化）
    void destroy_vm(uint64_t vm_id);
    
    // 中断管理接口 - 所有 VM 的中断都通过这里
    void handle_vm_interrupt_request(const Message& msg);
    void handle_interrupt_result(const Message& msg);
    
    // 启动/停止管理器
    void start();
    void stop();
    
    // 状态查询
    size_t get_registered_vm_count() const;
    std::vector<uint64_t> get_all_vm_ids() const;
    
    // 获取线程池引用（可选，供高级用户使用）
    ThreadPool& get_lifecycle_pool() { return vm_lifecycle_pool_; }
    
    // 获取中断统计信息
    struct InterruptStats {
        size_t total_requests = 0;
        size_t pending_requests = 0;
        size_t completed_requests = 0;
        size_t timeout_requests = 0;
        double avg_latency_ms = 0.0;
    };
    InterruptStats get_interrupt_stats() const;
    
    // 公开 PendingInterrupt 供测试使用
    struct PendingInterruptPublic {
        uint64_t vm_id;
        int periph_id;
        InterruptType interrupt_type;
        InterruptPriority priority;
        int timeout_ms;
        std::chrono::steady_clock::time_point request_time;
        std::chrono::steady_clock::time_point enqueue_time;
        
        // 用于优先级队列比较（优先级高的在前）
        bool operator<(const PendingInterruptPublic& other) const {
            if (static_cast<int>(priority) != static_cast<int>(other.priority)) {
                return static_cast<int>(priority) < static_cast<int>(other.priority);
            }
            // 同优先级时，先入队的优先
            return enqueue_time > other.enqueue_time;
        }
    };

private:
    VmManager() = default;
    ~VmManager();
    
    // 内部数据结构
    std::unordered_map<uint64_t, std::shared_ptr<baseVM>> vms_;
    mutable std::mutex vm_mtx_;
    
    // VM 创建/销毁线程池（异步处理，避免阻塞主线程）
    ThreadPool vm_lifecycle_pool_{2};  // 2 个工作线程
    
    // ===== 增强的中断处理相关 =====
    
    // 待处理中断请求（按优先级组织）
    struct PendingInterrupt {
        uint64_t vm_id;
        int periph_id;
        InterruptType interrupt_type;
        InterruptPriority priority;
        int timeout_ms;
        std::chrono::steady_clock::time_point request_time;
        std::chrono::steady_clock::time_point enqueue_time;
        TimeoutManager::TimeoutId timeout_id = 0;  // TimeoutManager 的超时 ID
        
        // 用于优先级队列比较（优先级高的在前）
        bool operator<(const PendingInterrupt& other) const {
            if (static_cast<int>(priority) != static_cast<int>(other.priority)) {
                return static_cast<int>(priority) < static_cast<int>(other.priority);
            }
            // 同优先级时，先入队的优先
            return enqueue_time > other.enqueue_time;
        }
    };
    
    // 优先级中断队列
    std::priority_queue<PendingInterrupt> interrupt_priority_queue_;
    std::unordered_map<uint64_t, PendingInterrupt> pending_interrupts_;  // 按 VM ID 索引
    mutable std::mutex interrupt_mtx_;
    
    // 异步中断处理结果队列
    struct AsyncInterruptResult {
        uint64_t vm_id;
        InterruptResult result;
        std::chrono::steady_clock::time_point request_time;
    };
    std::queue<AsyncInterruptResult> async_result_queue_;
    mutable std::mutex async_result_mtx_;
    
    // 中断统计信息
    struct InterruptStatsInternal {
        std::atomic<size_t> total_requests{0};
        std::atomic<size_t> completed_requests{0};
        std::atomic<size_t> timeout_requests{0};
        std::atomic<int64_t> total_latency_ms{0};
    } interrupt_stats_;
    
    // 工作线程
    std::atomic<bool> running_{false};
    std::thread worker_thread_;
    std::condition_variable worker_cv_;
    mutable std::mutex worker_mtx_;
    
    // 私有方法
    void worker_loop();
    void forward_interrupt_to_router(const Message& msg);
    void process_interrupt_timeout(const PendingInterrupt& pending);
    void process_async_result(AsyncInterruptResult& async_result);
    
    // 消息处理器
    void on_interrupt_request(const Message& msg);
    void on_interrupt_result(const Message& msg);
};

// 全局快捷函数
inline void vm_manager_register_vm(std::shared_ptr<baseVM> vm) {
    VmManager::instance().register_vm(std::move(vm));
}

inline void vm_manager_unregister_vm(uint64_t vm_id) {
    VmManager::instance().unregister_vm(vm_id);
}

#endif // LIVS_VM_MANAGER_H
