#ifndef LIVS_PERIPH_TERMINAL_MANAGER_H
#define LIVS_PERIPH_TERMINAL_MANAGER_H

#include <cstdint>
#include <string>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <map>
#include "router/MessageProtocol.h"

// ✅ 新增：终端控制台管理器
// 负责管理 VM 的输入输出，支持多路复用和轮询机制

class TerminalManager {
public:
    TerminalManager();
    ~TerminalManager();
    
    // 初始化终端（分配终端 ID）
    int initialize_terminal(uint64_t vm_id);
    
    // 释放终端资源
    void release_terminal(int terminal_id);
    
    // ===== 输入管理 =====
    // 向终端写入输入数据（来自物理终端或模拟器）
    void write_input(int terminal_id, const std::string& data);
    
    // VM 从终端读取数据（阻塞式）
    std::string read_input(int terminal_id, uint64_t vm_id, int timeout_ms = 2000);
    
    // 检查是否有可用输入（非阻塞）
    bool has_input(int terminal_id) const;
    
    // 获取可用输入长度
    size_t input_available(int terminal_id) const;
    
    // ===== 输出管理 =====
    // VM 向终端写入输出数据
    void write_output(int terminal_id, const std::string& data);
    
    // 读取终端输出数据（用于显示或日志）
    std::string read_output(int terminal_id);
    
    // 清空终端输出缓冲区
    void clear_output(int terminal_id);
    
    // ===== 系统调用处理 =====
    // 处理 SYS_WRITE 系统调用
    int handle_sys_write(int terminal_id, const std::string& data);
    
    // 处理 SYS_READ 系统调用
    int handle_sys_read(int terminal_id, char* buffer, size_t count);
    
    // 处理 SYS_POLL 系统调用（轮询多个终端）
    int handle_sys_poll(const std::vector<int>& terminal_ids, int timeout_ms);
    
    // ===== 状态查询 =====
    // 获取终端状态信息
    struct TerminalStatus {
        int terminal_id;
        uint64_t owner_vm_id;
        size_t input_queue_size;
        size_t output_queue_size;
        bool active;
    };
    
    TerminalStatus get_status(int terminal_id) const;
    
    // 获取所有终端状态
    std::vector<TerminalStatus> get_all_statuses() const;
    
private:
    struct TerminalInfo {
        int id;
        uint64_t owner_vm_id;
        std::queue<std::string> input_queue;      // 输入缓冲区
        std::queue<std::string> output_queue;     // 输出缓冲区
        mutable std::mutex mtx;                   // 保护缓冲区
        std::condition_variable input_cv;         // 输入就绪条件变量
        bool active = false;
    };
    
    std::map<int, std::unique_ptr<TerminalInfo>> terminals_;
    mutable std::mutex global_mtx_;
    int next_terminal_id_;
    
    // 工具函数
    TerminalInfo* get_terminal(int terminal_id);
    const TerminalInfo* get_terminal(int terminal_id) const;
};

#endif // LIVS_PERIPH_TERMINAL_MANAGER_H
