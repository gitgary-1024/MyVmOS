#ifndef LIVS_PERIPH_DISK_H
#define LIVS_PERIPH_DISK_H

#include "PeriphManager.h"
#include <string>

class Disk {
public:
    explicit Disk(PeriphManager* pm);
    void handle_interrupt_request(const Message& msg);

private:
    PeriphManager* pm_;
    // 模拟磁盘数据（简化）
    std::string sectors_[10]; // 10 个扇区
};

#endif // LIVS_PERIPH_DISK_H