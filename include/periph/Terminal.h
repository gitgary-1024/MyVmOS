#ifndef LIVS_PERIPH_TERMINAL_H
#define LIVS_PERIPH_TERMINAL_H

#include "PeriphManager.h"
#include <string>

class Terminal {
public:
    explicit Terminal(PeriphManager* pm);
    void handle_interrupt_request(const Message& msg);

private:
    PeriphManager* pm_;
    std::string input_buffer_;
    std::string output_buffer_;
};

#endif // LIVS_PERIPH_TERMINAL_H
