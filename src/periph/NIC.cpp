#include "periph/NIC.h"
#include "periph/cross_platform.h"
#include "router/RouterCore.h"
#include "log/Logging.h"
#include <iostream>
#include <cstring>

NIC::NIC(PeriphManager* pm) : pm_(pm), socket_fd(INVALID_SOCKET_FD) {
    pm_->register_peripheral(2, "NIC");
    
    // 初始化网络库
    NET_INIT();
    
    // 创建 UDP socket（简化示例）
    socket_fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (socket_fd == INVALID_SOCKET_FD) {
        LOG_ERR_MOD("NIC", "Failed to create socket");
    } else {
        LOG_INFO_MOD("NIC", "Socket created successfully");
        
        // 绑定到本地端口（示例用 9000 端口）
        struct sockaddr_in local_addr;
        memset(&local_addr, 0, sizeof(local_addr));
        local_addr.sin_family = AF_INET;
        local_addr.sin_port = htons(9000);
        local_addr.sin_addr.s_addr = INADDR_ANY;
        
        if (bind(socket_fd, (struct sockaddr*)&local_addr, sizeof(local_addr)) < 0) {
            std::cerr << "[NIC] Failed to bind socket" << std::endl;
            SOCKET_CLOSE(socket_fd);
            socket_fd = INVALID_SOCKET_FD;
        } else {
            LOG_INFO_MOD("NIC", "Socket bound to port 9000");
        }
    }
    
    route_subscribe(MessageType::INTERRUPT_REQUEST, [this](const Message& msg) {
        this->handle_interrupt_request(msg);
    });
}

NIC::~NIC() {
    if (socket_fd != INVALID_SOCKET_FD) {
        SOCKET_CLOSE(socket_fd);
    }
    NET_CLEANUP();
}

void NIC::handle_interrupt_request(const Message& msg) {
    if (!msg.is_payload<InterruptRequest>()) return;
    auto* req = msg.get_payload<InterruptRequest>();
    
    if (req->periph_id != 2) return;

    // 真实网络操作
    if (req->interrupt_type == InterruptType::NETWORK_RECV) {
        // RECV 操作：从网络接收数据
        handle_recv(req->vm_id);
    } else if (req->interrupt_type == InterruptType::NETWORK_SEND) {
        // SEND 操作：发送数据到网络
        handle_send(req->vm_id, msg.sender_id);
    }
}

void NIC::handle_recv(uint64_t vm_id) {
    LOG_INFO_MOD("NIC", (std::string("Handling RECV request from VM ") + std::to_string(vm_id)).c_str());
    
    if (socket_fd == INVALID_SOCKET_FD) {
        LOG_ERR_MOD("NIC", "Socket not available");
        send_result(vm_id, -1, false);
        return;
    }
    
    // 非阻塞接收数据（简化示例）
    char buffer[1024];
    struct sockaddr_in remote_addr;
    socklen_t addr_len = sizeof(remote_addr);
    memset(&remote_addr, 0, sizeof(remote_addr));
    
    // 设置超时（简化处理）
#ifdef PLATFORM_WINDOWS
    u_long mode = 1;  // 非阻塞模式
    ioctlsocket(socket_fd, FIONBIO, &mode);
#else
    int flags = fcntl(socket_fd, F_GETFL, 0);
    fcntl(socket_fd, F_SETFL, flags | O_NONBLOCK);
#endif
    
    int bytes_received = recvfrom(socket_fd, buffer, sizeof(buffer), 0,
                                   (struct sockaddr*)&remote_addr, &addr_len);
    
    if (bytes_received > 0) {
        LOG_INFO_MOD("NIC", (std::string("Received ") + std::to_string(bytes_received) + " bytes").c_str());
        send_result(vm_id, bytes_received, false);
    } else {
        // 没有数据或错误，返回 0 表示无数据
        LOG_INFO_MOD("NIC", "No data received");
        send_result(vm_id, 0, false);
    }
}

void NIC::handle_send(uint64_t vm_id, uint64_t sender_id) {
    LOG_INFO_MOD("NIC", (std::string("Handling SEND request from VM ") + std::to_string(vm_id)).c_str());
    
    if (socket_fd == INVALID_SOCKET_FD) {
        LOG_ERR_MOD("NIC", "Socket not available");
        send_result(sender_id, -1, false);
        return;
    }
    
    // 发送数据（简化示例，实际应该从 payload 获取数据）
    const char* test_data = "Hello from VM";
    
    struct sockaddr_in dest_addr;
    memset(&dest_addr, 0, sizeof(dest_addr));
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(9001);  // 发送到 9001 端口
    dest_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    
    int bytes_sent = sendto(socket_fd, test_data, strlen(test_data), 0,
                             (struct sockaddr*)&dest_addr, sizeof(dest_addr));
    
    if (bytes_sent > 0) {
        LOG_INFO_MOD("NIC", (std::string("Sent ") + std::to_string(bytes_sent) + " bytes").c_str());
        send_result(sender_id, bytes_sent, false);
    } else {
        LOG_ERR_MOD("NIC", "Failed to send data");
        send_result(sender_id, -1, false);
    }
}

void NIC::send_result(uint64_t vm_id, int result, bool is_timeout) {
    InterruptResult res{vm_id, result, is_timeout};
    Message result_msg(2, MODULE_VM_MANAGER, MessageType::INTERRUPT_RESULT_READY);
    result_msg.set_payload(res);
    
    // 在新线程中发送消息，避免死锁
    std::thread([result_msg]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        route_send(result_msg);
    }).detach();
}