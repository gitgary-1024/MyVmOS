#ifndef LIVS_VM_MANAGER_H
#define LIVS_VM_MANAGER_H

#include <unordered_map>
#include <vector>
#include <mutex>
#include <memory>
#include <atomic>
#include <thread>
#include <condition_variable>
#include "../router/MessageProtocol.h"

// 前置声明
class baseVM;

// VM管理器：作为所有VM的统一中断入口点
class VmManager {
public:
    static VmManager& instance();
    
    // VM生命周期管理
    void register_vm(std::shared_ptr<baseVM> vm);
    void unregister_vm(uint64_t vm_id);
    std::shared_ptr<baseVM> get_vm(uint64_t vm_id);
    
    // 中断管理接口 - 所有VM的中断都通过这里
    void handle_vm_interrupt_request(const Message& msg);
    void handle_interrupt_result(const Message& msg);
    
    // 启动/停止管理器
    void start();
    void stop();
    
    // 状态查询
    size_t get_registered_vm_count() const;
    std::vector<uint64_t> get_all_vm_ids() const;

private:
    VmManager() = default;
    ~VmManager();
    
    // 内部数据结构
    std::unordered_map<uint64_t, std::shared_ptr<baseVM>> vms_;
    mutable std::mutex vm_mtx_;
    
    // 中断处理相关
    struct PendingInterrupt {
        uint64_t vm_id;
        int periph_id;
        MessageType interrupt_type;
        int timeout_ms;
        std::chrono::steady_clock::time_point request_time;
    };
    
    std::unordered_map<uint64_t, PendingInterrupt> pending_interrupts_;
    mutable std::mutex interrupt_mtx_;
    
    // 工作线程
    std::atomic<bool> running_{false};
    std::thread worker_thread_;
    std::condition_variable worker_cv_;
    mutable std::mutex worker_mtx_;
    
    // 私有方法
    void worker_loop();
    void forward_interrupt_to_router(const Message& msg);
    void process_interrupt_timeout(const PendingInterrupt& pending);
    
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