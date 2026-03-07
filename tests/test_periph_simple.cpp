#include <iostream>
#include <vector>
#include <thread>
#include <chrono>
#include <memory>
#include "vm/mVM.h"
#include "vm/VmManager.h"
#include "periph/PeriphManager.h"
#include "periph/Disk.h"
#include "periph/NIC.h"
#include "periph/Terminal.h"
#include "router/RouterCore.h"

// 全局变量记录中断返回值
int last_interrupt_value = 0;

// 模拟 InterruptScheduler 的回调
void mock_interrupt_handler(const Message& msg) {
    std::cout << "[Mock ISR] Received message type: " << static_cast<int>(msg.type) << std::endl;
    if (msg.type == MessageType::INTERRUPT_RESULT_READY && msg.is_payload<InterruptResult>()) {
        auto* res = msg.get_payload<InterruptResult>();
        last_interrupt_value = res->return_value;
        std::cout << "[ISR] VM " << res->vm_id << " got interrupt result: " << res->return_value << std::endl;
    } else {
        std::cout << "[Mock ISR] Not INTERRUPT_RESULT_READY or wrong payload" << std::endl;
    }
}

// 简单的中断调度器：转发 INTERRUPT_REQUEST 到对应外设
void interrupt_scheduler_handler(const Message& msg) {
    std::cout << "********************************************************************************" << std::endl;
    std::cout << "************************* INTERRUPT SCHEDULER CALLED ***************************" << std::endl;
    std::cout << "********************************************************************************" << std::endl;
    std::cout << "[InterruptScheduler] Handler called" << std::endl;
    std::cout << "[InterruptScheduler] Received message type: " << static_cast<int>(msg.type) << std::endl;
    if (msg.type == MessageType::INTERRUPT_REQUEST && msg.is_payload<InterruptRequest>()) {
        auto* req = msg.get_payload<InterruptRequest>();
        std::cout << "[InterruptScheduler] Got INTERRUPT_REQUEST: VM " << req->vm_id 
                  << " -> periph " << req->periph_id << std::endl;
        // 直接转发给对应外设（这里简单处理，实际应该更复杂）
        std::cout << "[InterruptScheduler] Forwarding to periph " << req->periph_id << std::endl;
        // 在新线程中发送消息，避免死锁
        std::thread([msg]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            route_send(msg);
            std::cout << "[InterruptScheduler] Forwarding completed in separate thread" << std::endl;
        }).detach();
        std::cout << "[InterruptScheduler] Started separate thread for forwarding" << std::endl;
    } else {
        std::cout << "[InterruptScheduler] Not INTERRUPT_REQUEST or wrong payload type" << std::endl;
    }
    // 显示所有收到的消息类型
    std::cout << "[InterruptScheduler] Message details: type=" << static_cast<int>(msg.type)
              << ", sender=" << msg.sender_id << ", receiver=" << msg.receiver_id << std::endl;
    std::cout << "[InterruptScheduler] Handler completed" << std::endl;
    std::cout << "********************************************************************************" << std::endl;
}

int main() {
    std::cout << "=== Simple Peripheral Test: VM INPUT -> Terminal ===" << std::endl;

    // 1. 启动 Router 和 VM Manager（polling 模式）
    auto& router = RouterCore::instance();
    auto& vm_manager = VmManager::instance();
    std::cout << "Starting router (polling mode) and VM manager..." << std::endl;
    router.start_polling();
    vm_manager.start();
    std::cout << "Router (polling) and VM manager started" << std::endl;

    // 2. 创建外设管理器和外设
    auto pm = std::make_unique<PeriphManager>(3, 1); // 3个外设，1通道
    auto disk = std::make_unique<Disk>(pm.get());
    auto nic = std::make_unique<NIC>(pm.get());
    auto term = std::make_unique<Terminal>(pm.get());

    // 3. 订阅中断调度和结果
    std::cout << "Subscribing to INTERRUPT_REQUEST and INTERRUPT_RESULT_READY" << std::endl;
    route_subscribe(MessageType::INTERRUPT_REQUEST, interrupt_scheduler_handler);
    route_subscribe(MessageType::INTERRUPT_RESULT_READY, mock_interrupt_handler);
    std::cout << "Subscriptions completed" << std::endl;
    // 给订阅一些时间建立
    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    // 4. 创建 VM (ID=1) - 并注册到 VM 管理器
    auto vm = std::make_shared<mVM>(1);
    vm_manager.register_vm(vm);  // 注册 VM
    vm->set_register(1, 0); // R1 = 0 (unused in this test)
    std::cout << "Initial: R0=" << vm->get_register(0) << std::endl;
    std::cout << "Initial state: " << static_cast<int>(vm->get_state()) << std::endl;

    // 程序：INPUT R0  → opcode=2, r1=0
    // 编码：opcode(8bit) | r1(8bit) | r2(16bit)
    // INPUT R0: opcode=2, r1=0, r2=0 → 0x00000002
    // HALT: opcode=4, r1=0, r2=0 → 0x00000004
    std::vector<uint32_t> prog = { 0x00000002, 0x00000004 }; // INPUT R0; HALT
    vm->load_program(prog);
    std::cout << "Program loaded, starting VM..." << std::endl;
    vm->start();

    // 5. 等待 VM 执行到 INPUT 并挂起
    std::cout << "Before sleep: state=" << static_cast<int>(vm->get_state()) << std::endl;
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    std::cout << "After INPUT: state=" << static_cast<int>(vm->get_state()) 
              << " (should be 2=SUSPENDED_WAITING_INTERRUPT)" << std::endl;
    std::cout << "PC = " << vm->get_pc() << std::endl;
    
    // 再等一会儿看状态是否改变
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    std::cout << "Later state: " << static_cast<int>(vm->get_state()) << std::endl;
    std::cout << "R0 = " << vm->get_register(0) << std::endl;

    // 6. 等待 Terminal 处理中断（它已订阅，会自动发送 INTERRUPT_RESULT_READY）
    std::cout << "Waiting for interrupt processing..." << std::endl;
    for (int i = 0; i < 30; i++) {  // 增加等待次数
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        std::cout << "Wait iteration " << i << ", R0 = " << vm->get_register(0) 
                  << ", state = " << static_cast<int>(vm->get_state()) << std::endl;
        if (vm->get_register(0) != 0) {
            std::cout << "SUCCESS: R0 updated to " << vm->get_register(0) << std::endl;
            break;
        }
    }

    // 7. 检查结果
    std::cout << "=== FINAL RESULTS ===" << std::endl;
    std::cout << "Final R0 = " << vm->get_register(0) << " (expected 65 for 'A')" << std::endl;
    std::cout << "Last interrupt value = " << last_interrupt_value << std::endl;
    std::cout << "VM state = " << static_cast<int>(vm->get_state()) << std::endl;
    std::cout << "=====================" << std::endl;

    vm->stop();
    vm_manager.stop();
    router.stop();
    return 0;
}