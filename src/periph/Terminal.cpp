#include "periph/Terminal.h"
#include "periph/cross_platform.h"
#include "router/RouterCore.h"
#include "log/Logging.h"
#include <iostream>
#include <sstream>

Terminal::Terminal(PeriphManager* pm) : pm_(pm) {
    pm_->register_peripheral(3, "Terminal");
    route_subscribe(MessageType::INTERRUPT_REQUEST, [this](const Message& msg) {
        this->handle_interrupt_request(msg);
    });
}

void Terminal::handle_interrupt_request(const Message& msg) {
    if (!msg.is_payload<InterruptRequest>()) {
        LOG_INFO_MOD("Terminal", "Not an InterruptRequest payload");
        return;
    }
    auto* req = msg.get_payload<InterruptRequest>();
    
    if (req->periph_id != 3) {
        LOG_INFO_MOD("Terminal", "Wrong periph_id, expected 3");
        return;
    }

    // 真实终端处理
    if (req->interrupt_type == InterruptType::TERMINAL_INPUT) {
        // INPUT 操作：从终端读取字符
        handle_input(req->vm_id);
    } else if (req->interrupt_type == InterruptType::TERMINAL_OUTPUT) {
        // OUTPUT 操作：输出到终端
        handle_output(req->vm_id, msg.sender_id);
    }
}

void Terminal::handle_input(uint64_t vm_id) {
    LOG_INFO_MOD("Terminal", "Reading input from user...");
    
    // 使用跨平台无回显输入
    char ch = GETCH();
    int result = static_cast<int>(ch);
    
    LOG_INFO_MOD("Terminal", (std::string("Got input: '") + ch + "' (ASCII: " + std::to_string(result) + ")").c_str());
    
    // 发送中断结果
    send_result(vm_id, result, false);
}

void Terminal::handle_output(uint64_t vm_id, uint64_t sender_id) {
    // OUTPUT 操作：输出到终端（带颜色）
    TERM_SET_COLOR(TERM_COLOR_GREEN);
    std::string output_msg = "<<< VM " + std::to_string(vm_id) + " output: ";
    if (output_buffer_.empty()) {
        output_msg += "(no data)";
    } else {
        output_msg += output_buffer_;
    }
    LOG_INFO_MOD("Terminal", output_msg.c_str());
    TERM_SET_COLOR(TERM_COLOR_RESET);
    
    // 发送完成确认
    send_result(sender_id, 0, false);
}

void Terminal::send_result(uint64_t vm_id, int result, bool is_timeout) {
    InterruptResult res{vm_id, result, is_timeout};
    Message result_msg(3, MODULE_VM_MANAGER, MessageType::INTERRUPT_RESULT_READY);
    result_msg.set_payload(res);
    
    // 在新线程中发送消息，避免死锁
    std::thread([result_msg]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        route_send(result_msg);
    }).detach();
}