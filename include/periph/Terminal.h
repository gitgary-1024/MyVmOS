#ifndef LIVS_PERIPH_TERMINAL_H
#define LIVS_PERIPH_TERMINAL_H

#include "PeriphManager.h"
#include <string>

class Terminal {
public:
    explicit Terminal(PeriphManager* pm);
    void handle_interrupt_request(const Message& msg);

private:
    // 真实终端处理函数
    void handle_input(uint64_t vm_id);
    void handle_output(uint64_t vm_id, uint64_t sender_id);
    void send_result(uint64_t vm_id, int result, bool is_timeout);
    
    PeriphManager* pm_;
    std::string input_buffer_;   // 输入缓冲区
    std::string output_buffer_;  // 输出缓冲区
};

#endif // LIVS_PERIPH_TERMINAL_H
