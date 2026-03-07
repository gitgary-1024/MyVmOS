#ifndef LIVS_PERIPH_NIC_H
#define LIVS_PERIPH_NIC_H

#include "PeriphManager.h"
#include <vector>
#include <string>

class NIC {
public:
    explicit NIC(PeriphManager* pm);
    void handle_interrupt_request(const Message& msg);

private:
    PeriphManager* pm_;
    std::vector<std::string> rx_buffer_;
    std::vector<std::string> tx_buffer_;
};

#endif // LIVS_PERIPH_NIC_H