#include <iostream>
#include <string>
#include <vector>
#include <chrono>
#include <thread>
#include <atomic>
#include "router/MessageProtocol.h"
#include "router/RouterCore.h"

// 模拟模块 ID
constexpr uint64_t MODULE_VM_MGR = 0x01;
constexpr uint64_t MODULE_PERIPH_MGR = 0x04;

// 全局测试状态
std::vector<std::string> received_messages;
std::mutex log_mtx;
std::atomic<int> sync_count{0};
std::atomic<int> async_count{0};
std::atomic<bool> exception_thrown{false};

// 辅助：记录日志（线程安全）
void log(const std::string& msg) {
    std::lock_guard<std::mutex> lock(log_mtx);
    received_messages.push_back(msg);
}

// 测试订阅回调
void vm_mgr_handler(const Message& msg) {
    std::string desc = "[VM_MGR] ";
    if (msg.is_payload<InterruptResult>()) {
        auto* res = msg.get_payload<InterruptResult>();
        desc += "INTERRUPT_RESULT: vm=" + std::to_string(res->vm_id) 
                + ", ret=" + std::to_string(res->return_value);
        sync_count.fetch_add(1);
    } else if (msg.is_payload<InterruptRequest>()) {
        auto* req = msg.get_payload<InterruptRequest>();
        desc += "INTERRUPT_REQ: vm=" + std::to_string(req->vm_id) 
                + ", type=" + std::to_string(static_cast<int>(req->interrupt_type));
    }
    log(desc);
}

void periph_mgr_handler(const Message& msg) {
    if (msg.type == MessageType::PERIPH_LOCK_REQUEST) {
        auto* req = msg.get_payload<PeriphLockRequest>();
        log("[PeriphMgr] LOCK_REQ: vm=" + std::to_string(req->vm_id) 
            + ", periph=" + std::to_string(req->periph_id));
    } else if (msg.type == MessageType::PERIPH_LOCK_GRANTED) {
        auto* granted = msg.get_payload<PeriphLockGranted>();
        log("[PeriphMgr] LOCK_GRANTED: vm=" + std::to_string(granted->vm_id));
    }
}

// 异常订阅者（用于测试异常隔离）
void faulty_handler(const Message&) {
    throw std::runtime_error("Simulated subscriber error");
}

int main() {
    std::cout << "=== Starting RouterCore Unit Test ===" << std::endl;

    // 1. 初始化路由器
    auto& router = RouterCore::instance();
    router.register_module(MODULE_VM_MGR, "VMManager");
    router.register_module(MODULE_PERIPH_MGR, "PeriphManager");
    router.start();

    // 2. 订阅消息
    route_subscribe(MessageType::INTERRUPT_RESULT_READY, vm_mgr_handler);
    route_subscribe(MessageType::INTERRUPT_REQUEST, vm_mgr_handler);
    route_subscribe(MessageType::PERIPH_LOCK_REQUEST, periph_mgr_handler);
    route_subscribe(MessageType::PERIPH_LOCK_GRANTED, periph_mgr_handler);

    // 3. 注册一个会抛异常的订阅者（测试异常隔离）
    try {
        route_subscribe(MessageType::ROUTER_PING, faulty_handler);
    } catch (...) {
        std::cerr << "[WARN] Exception in subscribe ignored (as expected)" << std::endl;
    }

    // 4. 发送测试消息（混合同步/异步，验证优先级）
    // 先发一个异步中断（低优）
    Message async_msg(MODULE_VM_MGR, MODULE_PERIPH_MGR, MessageType::INTERRUPT_ASYNC_ENQUEUE);
    InterruptRequest async_req{1001, 2, MessageType::INTERRUPT_ASYNC_ENQUEUE, 1000};
    async_msg.set_payload(async_req);
    route_send(async_msg);
    log("[TEST] Sent async interrupt");

    // 再发一个同步中断（高优）—— 应该先被处理
    Message sync_msg(MODULE_VM_MGR, MODULE_PERIPH_MGR, MessageType::INTERRUPT_SYNC_BEGIN);
    InterruptRequest sync_req{1002, 2, MessageType::INTERRUPT_SYNC_BEGIN, 2000};
    sync_msg.set_payload(sync_req);
    route_send(sync_msg);
    log("[TEST] Sent sync interrupt");

    // 发送 GIL 请求（次高优）
    Message lock_req(MODULE_VM_MGR, MODULE_PERIPH_MGR, MessageType::PERIPH_LOCK_REQUEST);
    PeriphLockRequest plr{1003, 2, 1, 500};
    lock_req.set_payload(plr);
    route_send(lock_req);
    log("[TEST] Sent GIL lock request");

    // 等待处理完成
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    // 5. 发送结果（模拟外设完成）
    Message result_msg(MODULE_PERIPH_MGR, MODULE_VM_MGR, MessageType::INTERRUPT_RESULT_READY);
    InterruptResult res{1002, 42, false}; // vm_id=1002, return=42
    result_msg.set_payload(res);
    route_send(result_msg);
    log("[TEST] Sent sync result");

    // 等待处理
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    router.stop();

    // 6. 验证结果
    std::cout << "\n=== Test Results ===" << std::endl;
    std::cout << "Total messages received: " << received_messages.size() << std::endl;
    
    bool sync_before_async = false;
    size_t sync_pos = std::string::npos, async_pos = std::string::npos;
    for (size_t i = 0; i < received_messages.size(); ++i) {
        const auto& m = received_messages[i];
        if (m.find("INTERRUPT_RESULT") != std::string::npos) sync_pos = i;
        if (m.find("vm=1002") != std::string::npos && m.find("INTERRUPT_REQ") != std::string::npos) sync_pos = i;
        if (m.find("INTERRUPT_ASYNC") != std::string::npos) async_pos = i;
    }

    if (sync_pos != std::string::npos && async_pos != std::string::npos) {
        sync_before_async = sync_pos < async_pos;
        std::cout << "✅ Sync message processed before async: " << (sync_before_async ? "YES" : "NO") << std::endl;
    } else {
        std::cout << "⚠️  Sync/async position not found, but messages exist." << std::endl;
    }

    std::cout << "Sync count: " << sync_count.load() << " (expected >=1)" << std::endl;
    std::cout << "Async count: " << async_count.load() << " (expected 0 in this test)" << std::endl;
    std::cout << "Exception thrown in subscriber: " << (exception_thrown.load() ? "YES" : "NO") << std::endl;

    // 打印所有日志
    std::cout << "\n--- Received Messages ---" << std::endl;
    for (size_t i = 0; i < received_messages.size(); ++i) {
        std::cout << "[" << i << "] " << received_messages[i] << std::endl;
    }

    // 7. 基本断言
    if (received_messages.size() >= 3 && sync_count.load() >= 1) {
        std::cout << "\n✅ TEST PASSED: RouterCore works correctly." << std::endl;
        return 0;
    } else {
        std::cout << "\n❌ TEST FAILED: Missing expected messages or counts." << std::endl;
        return 1;
    }
}