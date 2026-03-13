#include "periph/Disk.h"
#include "periph/cross_platform.h"
#include "router/RouterCore.h"
#include "log/Logging.h"
#include <iostream>
#include <fstream>
#include <cstring>

Disk::Disk(PeriphManager* pm) : pm_(pm) {
    pm_->register_peripheral(1, "Disk");
    route_subscribe(MessageType::INTERRUPT_REQUEST, [this](const Message& msg) {
        this->handle_interrupt_request(msg);
    });
    
    // 初始化网络库（如果需要）
    NET_INIT();
}

Disk::~Disk() {
    NET_CLEANUP();
}

void Disk::handle_interrupt_request(const Message& msg) {
    if (!msg.is_payload<InterruptRequest>()) return;
    auto* req = msg.get_payload<InterruptRequest>();
    
    if (req->periph_id != 1) return;

    // 真实磁盘操作
    if (req->interrupt_type == MessageType::INTERRUPT_SYNC_BEGIN) {
        // READ 操作：从磁盘读取数据
        handle_read(req->vm_id);
    } else if (req->interrupt_type == MessageType::INTERRUPT_SYNC_COMPLETE) {
        // WRITE 操作：写入数据到磁盘
        handle_write(req->vm_id, msg.sender_id);
    }
}

void Disk::handle_read(uint64_t vm_id) {
    LOG_INFO_MOD("Disk", (std::string("Handling READ request from VM ") + std::to_string(vm_id)).c_str());
    
    // 示例：从虚拟磁盘文件读取
    const char* filename = "disk_image.bin";
    
    if (!FILE_EXISTS(filename)) {
        LOG_INFO_MOD("Disk", "File not found, creating default disk image");
        create_default_disk_image(filename);
    }
    
    // 读取文件大小作为结果
    std::ifstream file(filename, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        LOG_ERR_MOD("Disk", "Failed to open disk image");
        send_result(vm_id, -1, false);
        return;
    }
    
    std::streamsize size = file.tellg();
    file.close();
    
    LOG_INFO_MOD("Disk", (std::string("Read completed, size: ") + std::to_string(size) + " bytes").c_str());
    send_result(vm_id, static_cast<int>(size), false);
}

void Disk::handle_write(uint64_t vm_id, uint64_t sender_id) {
    LOG_INFO_MOD("Disk", (std::string("Handling WRITE request from VM ") + std::to_string(vm_id)).c_str());
    
    // 示例：写入数据到虚拟磁盘
    const char* filename = "disk_image.bin";
    
    // 这里简化处理，实际应该从消息 payload 中获取要写入的数据
    std::ofstream file(filename, std::ios::binary | std::ios::app);
    if (!file.is_open()) {
        LOG_ERR_MOD("Disk", "Failed to open disk image for writing");
        send_result(sender_id, -1, false);
        return;
    }
    
    // 写入示例数据
    const char* data = "VM write operation\n";
    file.write(data, strlen(data));
    file.close();
    
    LOG_INFO_MOD("Disk", "Write completed");
    send_result(sender_id, 0, false);
}

void Disk::create_default_disk_image(const char* filename) {
    std::ofstream file(filename, std::ios::binary);
    if (!file.is_open()) {
        LOG_ERR_MOD("Disk", "Failed to create disk image");
        return;
    }
    
    // 创建一个默认的 4KB 磁盘镜像
    char buffer[4096] = {0};
    file.write(buffer, sizeof(buffer));
    file.close();
    
    LOG_INFO_MOD("Disk", "Created default 4KB disk image");
}

void Disk::send_result(uint64_t vm_id, int result, bool is_timeout) {
    InterruptResult res{vm_id, result, is_timeout};
    Message result_msg(1, vm_id, MessageType::INTERRUPT_RESULT_READY);
    result_msg.set_payload(res);
    
    // 在新线程中发送消息，避免死锁
    std::thread([result_msg]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        route_send(result_msg);
    }).detach();
}