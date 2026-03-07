#include "periph/Terminal.h"
#include "router/RouterCore.h"
#include <iostream>

Terminal::Terminal(PeriphManager* pm) : pm_(pm) {
    pm_->register_peripheral(3, "Terminal");
    route_subscribe(MessageType::INTERRUPT_REQUEST, [this](const Message& msg) {
        this->handle_interrupt_request(msg);
    });
}

void Terminal::handle_interrupt_request(const Message& msg) {
    std::cout << "[Terminal] >>>>> handle_interrupt_request STARTED" << std::endl;
    if (!msg.is_payload<InterruptRequest>()) {
        std::cout << "[Terminal] Not an InterruptRequest payload" << std::endl;
        return;
    }
    auto* req = msg.get_payload<InterruptRequest>();
    std::cout << "[Terminal] Request for periph_id=" << req->periph_id << ", vm_id=" << req->vm_id << std::endl;
    
    if (req->periph_id != 3) {
        std::cout << "[Terminal] Wrong periph_id, expected 3" << std::endl;
        return;
    }

    std::cout << "[Terminal] Processing INPUT request from VM " << req->vm_id << std::endl;
    // 模拟终端：INPUT 返回字符 'A' (65)
    InterruptResult res{req->vm_id, 65, false};
    std::cout << "[Terminal] Original message sender_id: " << msg.sender_id << std::endl;
    // 正确的消息格式：sender=外设ID(3), receiver=VM ID(1)
    Message result_msg(3, req->vm_id, MessageType::INTERRUPT_RESULT_READY);
    result_msg.set_payload(res);
    std::cout << "[Terminal] Preparing to send INTERRUPT_RESULT_READY: sender=" << result_msg.sender_id 
              << ", receiver=" << result_msg.receiver_id << ", type=" << static_cast<int>(result_msg.type) << std::endl;
    std::cout << "[Terminal] >>>>> About to call route_send" << std::endl;
    
    // 在新线程中发送消息，避免死锁
    std::thread([result_msg]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        route_send(result_msg);
        std::cout << "[Terminal] <<<<< route_send completed in separate thread" << std::endl;
    }).detach();
    
    std::cout << "[Terminal] Started separate thread for route_send" << std::endl;

    std::cout << "[Terminal] Handled INT request from VM " << req->vm_id << ", returned 'A'" << std::endl;
    std::cout << "[Terminal] <<<<< handle_interrupt_request COMPLETED" << std::endl;
}