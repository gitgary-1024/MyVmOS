#include <iostream>
#include <vector>
#include <thread>
#include <chrono>
#include "vm/mVM.h"
#include "router/RouterCore.h"

// 全局变量记录中断返回值
int last_interrupt_value = 0;

// 模拟 InterruptScheduler 的回调：当收到 INTERRUPT_RESULT_READY 时，保存返回值
void mock_interrupt_handler(const Message& msg) {
    if (msg.type == MessageType::INTERRUPT_RESULT_READY && msg.is_payload<InterruptResult>()) {
        auto* res = msg.get_payload<InterruptResult>();
        last_interrupt_value = res->return_value;
        std::cout << "[Mock ISR] Interrupt result: vm=" << res->vm_id 
                  << ", return=" << res->return_value << std::endl;
    }
}

// 模拟 Scheduler 的唤醒处理（仅打印）
void mock_scheduler_handler(const Message& msg) {
    if (msg.type == MessageType::VM_WAKEUP_NOTIFY && msg.is_payload<VMWakeUpNotify>()) {
        auto* wake = msg.get_payload<VMWakeUpNotify>();
        std::cout << "[Scheduler] VM " << wake->vm_id << " woken up" << std::endl;
    }
}

int main() {
    std::cout << "=== Testing Simplified mVM: ADD, INPUT, OUTPUT ===" << std::endl;

    // 1. 启动 Router
    auto& router = RouterCore::instance();
    router.register_module(0x01, "VMManager");
    router.register_module(0x03, "InterruptScheduler");
    router.register_module(0x05, "Scheduler");
    router.start();

    // 2. 订阅中断结果和唤醒消息
    route_subscribe(MessageType::INTERRUPT_RESULT_READY, mock_interrupt_handler);
    route_subscribe(MessageType::VM_WAKEUP_NOTIFY, mock_scheduler_handler);

    // ========== 测试场景：INPUT R0 → ADD R0, R1 → OUTPUT R0 ===========
    // 目标：R1 = 5（手动设置），INPUT R0 → 返回 100 → R0=100；ADD R0,R1 → R0=105
    auto vm = std::make_unique<mVM>(1);

    // 手动初始化 R1 = 5（因无 LOAD 指令）
    vm->set_register(1, 5);
    std::cout << "Initial: R0=" << vm->get_register(0) << ", R1=" << vm->get_register(1) << std::endl;

    // 程序编码：
    // INPUT R0  → opcode=2, r1=0 → 0x02 00 00 → 0x00000200
    // ADD R0, R1 → opcode=1, r1=0, r2=1 → 0x01 00 01 → 0x01010001
    // OUTPUT R0 → opcode=3, r1=0 → 0x03 00 00 → 0x00000300
    // HALT      → opcode=4 → 0x00000004
    std::vector<uint32_t> prog = {
        0x00000200,  // INPUT R0
        0x01010001,  // ADD R0, R1
        0x00000300,  // OUTPUT R0
        0x00000004   // HALT
    };
    vm->load_program(prog);
    vm->start();

    // 等待 VM 执行到 INPUT 并挂起
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    std::cout << "After INPUT: state=" << static_cast<int>(vm->get_state()) 
              << " (should be 2=SUSPENDED_WAITING_INTERRUPT)" << std::endl;

    // 模拟中断返回：INPUT 的结果 = 100
    Message result_msg(0x03, 1, MessageType::INTERRUPT_RESULT_READY);
    InterruptResult res{1, 100, false};
    result_msg.set_payload(res);
    route_send(result_msg);

    // 等待 handle_interrupt 执行并发送唤醒
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    std::cout << "After interrupt return: R0=" << vm->get_register(0) << " (should be 100)" << std::endl;

    // 继续执行 ADD 和 OUTPUT
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    std::cout << "After ADD: R0=" << vm->get_register(0) << " (should be 105 = 100+5)" << std::endl;

    // 模拟 OUTPUT 的中断返回（无实际作用，但触发流程）
    Message result_msg2(0x03, 1, MessageType::INTERRUPT_RESULT_READY);
    InterruptResult res2{1, 0, false}; // OUTPUT 不关心返回值
    result_msg2.set_payload(res2);
    route_send(result_msg2);

    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    std::cout << "Final R0 = " << vm->get_register(0) << std::endl;
    vm->stop();
    router.stop();

    return 0;
}