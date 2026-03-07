#include "periph/NIC.h"
#include "router/RouterCore.h"
#include <iostream>

NIC::NIC(PeriphManager* pm) : pm_(pm) {
    pm_->register_peripheral(2, "NIC");
    route_subscribe(MessageType::INTERRUPT_REQUEST, [this](const Message& msg) {
        this->handle_interrupt_request(msg);
    });
}

void NIC::handle_interrupt_request(const Message& msg) {
    if (!msg.is_payload<InterruptRequest>()) return;
    auto* req = msg.get_payload<InterruptRequest>();
    
    if (req->periph_id != 2) return;

    // 模拟网络：INPUT 返回包长 64 字节
    InterruptResult res{req->vm_id, 64, false};
    Message result_msg(req->vm_id, msg.sender_id, MessageType::INTERRUPT_RESULT_READY);
    result_msg.set_payload(res);
    route_send(result_msg);

    std::cout << "[NIC] Handled INT request from VM " << req->vm_id << ", returned 64" << std::endl;
}