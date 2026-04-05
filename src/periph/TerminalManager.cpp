#include "TerminalManager.h"
#include <iostream>
#include <chrono>
#include <algorithm>
#include <thread>


TerminalManager::TerminalManager() : next_terminal_id_(1) {
    std::cout << "[TerminalManager] Initialized" << std::endl;
}

TerminalManager::~TerminalManager() {
    std::cout << "[TerminalManager] Cleanup completed" << std::endl;
}

int TerminalManager::initialize_terminal(uint64_t vm_id) {
    std::lock_guard<std::mutex> lock(global_mtx_);
    
    int terminal_id = next_terminal_id_++;
    terminals_[terminal_id] = std::make_unique<TerminalInfo>();
    
    auto& term_info = terminals_[terminal_id];
    term_info->id = terminal_id;
    term_info->owner_vm_id = vm_id;
    term_info->active = true;
    
    std::cout << "[TerminalManager] Terminal " << terminal_id 
              << " initialized for VM " << vm_id << std::endl;
    
    return terminal_id;
}

void TerminalManager::release_terminal(int terminal_id) {
    std::lock_guard<std::mutex> lock(global_mtx_);
    
    auto it = terminals_.find(terminal_id);
    if (it != terminals_.end()) {
        std::cout << "[TerminalManager] Releasing terminal " << terminal_id << std::endl;
        terminals_.erase(it);
    } else {
        std::cerr << "[TerminalManager] Terminal " << terminal_id << " not found" << std::endl;
    }
}

void TerminalManager::write_input(int terminal_id, const std::string& data) {
    auto* term_info = get_terminal(terminal_id);
    if (!term_info) {
        std::cerr << "[TerminalManager] Terminal " << terminal_id << " not found for write_input" << std::endl;
        return;
    }
    
    {
        std::lock_guard<std::mutex> lock(term_info->mtx);
        term_info->input_queue.push(data);
        std::cout << "[TerminalManager] Terminal " << terminal_id 
                  << " received input: \"" << data << "\"" << std::endl;
    }
    
    // 通知等待的读取者
    term_info->input_cv.notify_one();
}

std::string TerminalManager::read_input(int terminal_id, uint64_t vm_id, int timeout_ms) {
    auto* term_info = get_terminal(terminal_id);
    if (!term_info) {
        std::cerr << "[TerminalManager] Terminal " << terminal_id << " not found for read_input" << std::endl;
        return "";
    }
    
    std::unique_lock<std::mutex> lock(term_info->mtx);
    
    // 等待输入就绪或超时
    bool ready = term_info->input_cv.wait_for(lock, std::chrono::milliseconds(timeout_ms),
                                               [this, term_id = terminal_id]() {
                                                   return !terminals_[term_id]->input_queue.empty();
                                               });
    
    if (!ready || term_info->input_queue.empty()) {
        std::cout << "[TerminalManager] Terminal " << terminal_id 
                  << " read timeout after " << timeout_ms << "ms" << std::endl;
        return "";
    }
    
    std::string data = term_info->input_queue.front();
    term_info->input_queue.pop();
    
    std::cout << "[TerminalManager] Terminal " << terminal_id 
              << " read input: \"" << data << "\"" << std::endl;
    
    return data;
}

bool TerminalManager::has_input(int terminal_id) const {
    auto* term_info = get_terminal(terminal_id);
    if (!term_info) {
        return false;
    }
    
    std::lock_guard<std::mutex> lock(term_info->mtx);
    return !term_info->input_queue.empty();
}

size_t TerminalManager::input_available(int terminal_id) const {
    auto* term_info = get_terminal(terminal_id);
    if (!term_info) {
        return 0;
    }
    
    std::lock_guard<std::mutex> lock(term_info->mtx);
    return term_info->input_queue.size();
}

void TerminalManager::write_output(int terminal_id, const std::string& data) {
    auto* term_info = get_terminal(terminal_id);
    if (!term_info) {
        std::cerr << "[TerminalManager] Terminal " << terminal_id << " not found for write_output" << std::endl;
        return;
    }
    
    {
        std::lock_guard<std::mutex> lock(term_info->mtx);
        term_info->output_queue.push(data);
    }
    
    std::cout << "[TerminalManager] Terminal " << terminal_id 
              << " output: \"" << data << "\"" << std::endl;
}

std::string TerminalManager::read_output(int terminal_id) {
    auto* term_info = get_terminal(terminal_id);
    if (!term_info) {
        return "";
    }
    
    std::lock_guard<std::mutex> lock(term_info->mtx);
    
    if (term_info->output_queue.empty()) {
        return "";
    }
    
    std::string data = term_info->output_queue.front();
    term_info->output_queue.pop();
    
    return data;
}

void TerminalManager::clear_output(int terminal_id) {
    auto* term_info = get_terminal(terminal_id);
    if (!term_info) {
        return;
    }
    
    std::lock_guard<std::mutex> lock(term_info->mtx);
    
    // 清空输出队列
    while (!term_info->output_queue.empty()) {
        term_info->output_queue.pop();
    }
}

int TerminalManager::handle_sys_write(int terminal_id, const std::string& data) {
    write_output(terminal_id, data);
    return static_cast<int>(data.size());  // 返回写入的字节数
}

int TerminalManager::handle_sys_read(int terminal_id, char* buffer, size_t count) {
    std::string data = read_input(terminal_id, 0, 2000);
    
    if (data.empty()) {
        return -1;  // 超时或无数据
    }
    
    size_t bytes_to_copy = std::min(data.size(), count - 1);
    std::copy_n(data.begin(), bytes_to_copy, buffer);
    buffer[bytes_to_copy] = '\0';
    
    return static_cast<int>(bytes_to_copy);
}

int TerminalManager::handle_sys_poll(const std::vector<int>& terminal_ids, int timeout_ms) {
    auto start_time = std::chrono::steady_clock::now();
    
    while (true) {
        // 检查是否有终端有输入
        for (int term_id : terminal_ids) {
            if (has_input(term_id)) {
                return 1;  // 有数据可读
            }
        }
        
        // 检查是否超时
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start_time).count();
        
        if (elapsed >= timeout_ms) {
            return 0;  // 超时
        }
        
        // 短暂休眠，避免忙等
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

TerminalManager::TerminalStatus TerminalManager::get_status(int terminal_id) const {
    TerminalStatus status = {};
    auto* term_info = get_terminal(terminal_id);
    
    if (!term_info) {
        status.terminal_id = terminal_id;
        status.active = false;
        return status;
    }
    
    std::lock_guard<std::mutex> lock(term_info->mtx);
    
    status.terminal_id = term_info->id;
    status.owner_vm_id = term_info->owner_vm_id;
    status.input_queue_size = term_info->input_queue.size();
    status.output_queue_size = term_info->output_queue.size();
    status.active = term_info->active;
    
    return status;
}

std::vector<TerminalManager::TerminalStatus> TerminalManager::get_all_statuses() const {
    std::lock_guard<std::mutex> lock(global_mtx_);
    
    std::vector<TerminalStatus> statuses;
    for (const auto& [id, info] : terminals_) {
        statuses.push_back(get_status(id));
    }
    
    return statuses;
}

TerminalManager::TerminalInfo* TerminalManager::get_terminal(int terminal_id) {
    std::lock_guard<std::mutex> lock(global_mtx_);
    
    auto it = terminals_.find(terminal_id);
    if (it == terminals_.end()) {
        return nullptr;
    }
    
    return it->second.get();
}

const TerminalManager::TerminalInfo* TerminalManager::get_terminal(int terminal_id) const {
    std::lock_guard<std::mutex> lock(global_mtx_);
    
    auto it = terminals_.find(terminal_id);
    if (it == terminals_.end()) {
        return nullptr;
    }
    
    return it->second.get();
}
