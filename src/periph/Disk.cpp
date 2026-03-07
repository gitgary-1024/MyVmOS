#include "periph/Disk.h"
#include "router/RouterCore.h"
#include <iostream>

Disk::Disk(PeriphManager* pm) : pm_(pm) {
    pm_->register_peripheral(1, "Disk");
    // 订阅中断请求（由 RouterCore 调用）
    route_subscribe(MessageType::INTERRUPT_REQUEST, [this](const Message& msg) {
        this->handle_interrupt_request(msg);
    });
}

void Disk::handle_interrupt_request(const Message& msg) {
    if (!msg.is_payload<InterruptRequest>()) return;
    auto* req = msg.get_payload<InterruptRequest>();
    
    if (req->periph_id != 1) return;

    // 模拟磁盘操作
    InterruptResult res{req->vm_id, 4096, false}; // 扇区大小 4096 字节
    Message result_msg(req->vm_id, msg.sender_id, MessageType::INTERRUPT_RESULT_READY);
    result_msg.set_payload(res);
    route_send(result_msg);

    std::cout << "[Disk] Handled INT request from VM " << req->vm_id << ", returned 4096" << std::endl;
}