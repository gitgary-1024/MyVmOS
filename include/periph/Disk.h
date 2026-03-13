#ifndef LIVS_PERIPH_DISK_H
#define LIVS_PERIPH_DISK_H

#include "PeriphManager.h"

class Disk {
public:
    explicit Disk(PeriphManager* pm);
    ~Disk();
    
    void handle_interrupt_request(const Message& msg);

private:
    // 真实磁盘操作函数
    void handle_read(uint64_t vm_id);
    void handle_write(uint64_t vm_id, uint64_t sender_id);
    void create_default_disk_image(const char* filename);
    void send_result(uint64_t vm_id, int result, bool is_timeout);
    
    PeriphManager* pm_;
};

#endif // LIVS_PERIPH_DISK_H