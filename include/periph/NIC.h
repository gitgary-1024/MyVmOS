#ifndef LIVS_PERIPH_NIC_H
#define LIVS_PERIPH_NIC_H

#include "PeriphManager.h"
#include "periph/cross_platform.h"

class NIC {
public:
    explicit NIC(PeriphManager* pm);
    ~NIC();
    
    void handle_interrupt_request(const Message& msg);

private:
    // 真实网络操作函数
    void handle_recv(uint64_t vm_id);
    void handle_send(uint64_t vm_id, uint64_t sender_id);
    void send_result(uint64_t vm_id, int result, bool is_timeout);
    
    PeriphManager* pm_;
    SocketType socket_fd;  // Socket 句柄
};

#endif // LIVS_PERIPH_NIC_H