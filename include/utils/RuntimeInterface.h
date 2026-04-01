#ifndef RUNTIME_INTERFACE_H
#define RUNTIME_INTERFACE_H

#include "../vm/VmManager.h"
#include "../periph/PeriphManager.h"
#include <string>
#include <vector>
#include <functional>
#include <map>

/**
 * @brief 运行时交互接口模块
 * 
 * 功能：
 * 1. VM 管理：创建/销毁/查询 VM
 * 2. 外设控制：查询/配置外设
 * 3. 消息调试：发送/接收测试消息
 * 4. 系统监控：性能统计、状态查询
 * 5. 热插拔：动态添加/移除设备
 * 
 * 使用场景：
 * - 调试控制台
 * - 监控工具
 * - 自动化测试
 * - 远程控制接口
 */
class RuntimeInterface {
public:
    // ==================== 单例模式 ====================
    static RuntimeInterface& getInstance() {
        static RuntimeInterface instance;
        return instance;
    }
    
    // ==================== 初始化 ====================
    void initialize(VmManager* vmMgr, PeriphManager* periphMgr);
    void shutdown();
    
    // ==================== VM 管理接口 ====================
    
    /**
     * @brief 创建新 VM
     * @param type VM 类型 ("X86", "RISCV" 等)
     * @param config 配置参数
     * @return VM ID，失败返回 -1
     */
    int createVM(const std::string& type, const std::map<std::string, std::string>& config = {});
    
    /**
     * @brief 销毁 VM
     * @param vmId VM ID
     * @return 成功返回 true
     */
    bool destroyVM(int vmId);
    
    /**
     * @brief 获取 VM 状态
     */
    struct VMStatus {
        int id;
        std::string type;
        std::string state;      // "CREATED", "RUNNING", "WAITING_INTERRUPT", "TERMINATED", "ERROR", "NOT_FOUND"
        uint64_t instructions;  // 执行指令数
        uint64_t cycles;        // CPU 周期数
        size_t memory_used;     // 内存使用（字节）
        int peripheral_count;   // 绑定外设数量
        
        // 调度信息（新增）
        int priority = 0;           // 优先级
        bool has_quota = false;     // 是否有运行名额
        int blocked_reason = 0;     // 阻塞原因（0=无，1=等待外设，2=等待中断）
        
        // 错误信息（新增）
        std::string error_message;  // 当状态为 ERROR 或 NOT_FOUND 时包含详细信息
    };
    
    VMStatus getVMStatus(int vmId);
    std::vector<VMStatus> getAllVMsStatus();
    
    /**
     * @brief 控制 VM 运行
     */
    bool startVM(int vmId);
    bool stopVM(int vmId);
    bool pauseVM(int vmId);
    bool resumeVM(int vmId);
    
    /**
     * @brief 读取/修改 VM 寄存器
     */
    uint32_t readRegister(int vmId, const std::string& regName);
    void writeRegister(int vmId, const std::string& regName, uint32_t value);
    
    /**
     * @brief 读取/修改 VM 内存
     */
    void readMemory(int vmId, void* dest, size_t offset, size_t size);
    void writeMemory(int vmId, const void* src, size_t offset, size_t size);
    
    // ==================== 外设管理接口 ====================
    
    /**
     * @brief 获取外设状态
     */
    struct PeriphStatus {
        std::string name;
        std::string type;       // "Terminal", "Disk", "NIC"
        std::string state;      // "ACTIVE", "IDLE", "ERROR"
        size_t data_size;       // 数据大小（字节）
        bool has_pending_op;    // 是否有未完成操作
    };
    
    PeriphStatus getPeriphStatus(const std::string& name);
    std::vector<PeriphStatus> getAllPeriphsStatus();
    
    /**
     * @brief 控制外设
     */
    bool resetPeriph(const std::string& name);
    bool configurePeriph(const std::string& name, const std::map<std::string, std::string>& config);
    
    /**
     * @brief 外设数据传输
     */
    bool readPeriphData(const std::string& name, void* buffer, size_t size);
    bool writePeriphData(const std::string& name, const void* buffer, size_t size);
    
    // ==================== 消息调试接口 ====================
    
    /**
     * @brief 发送测试消息
     */
    bool sendTestMessage(int fromVmId, int toVmId, MessageType type, const void* payload, size_t size);
    
    /**
     * @brief 获取消息队列状态
     */
    struct MessageQueueStatus {
        size_t total_capacity;
        size_t current_usage;
        size_t pending_messages;  // 待处理消息数
        size_t dropped_messages;  // 丢弃消息数
    };
    
    MessageQueueStatus getMessageQueueStatus();
    
    // ==================== 系统监控接口 ====================
    
    /**
     * @brief 系统整体状态
     */
    struct SystemStatus {
        size_t total_vms;           // VM 总数
        size_t running_vms;         // 运行中 VM 数
        size_t total_peripherals;   // 外设总数
        size_t active_peripherals;  // 活跃外设数
        uint64_t total_instructions;// 总执行指令数
        uint64_t total_cycles;      // 总 CPU 周期
        size_t memory_usage;        // 总内存使用
        double uptime_seconds;      // 运行时间
    };
    
    SystemStatus getSystemStatus();
    
    /**
     * @brief 性能统计
     */
    struct PerformanceStats {
        double avg_vm_cpu_usage;    // VM 平均 CPU 使用率
        double messages_per_second; // 消息吞吐率
        double io_operations_per_second; // IO 操作频率
    };
    
    PerformanceStats getPerformanceStats();
    
    // ==================== 回调函数注册 ====================
    
    /**
     * @brief 注册状态变化回调
     */
    using StatusCallback = std::function<void(const std::string& event, int vmId, const std::string& details)>;
    void registerStatusCallback(StatusCallback callback);
    
    // ==================== 命令行接口（可选） ====================
    
    /**
     * @brief 执行命令（用于控制台）
     * @param command 命令字符串，如 "vm list", "vm create X86", "periph show Terminal0"
     * @return 命令输出
     */
    std::string executeCommand(const std::string& command);
    
private:
    RuntimeInterface() = default;
    ~RuntimeInterface() = default;
    RuntimeInterface(const RuntimeInterface&) = delete;
    RuntimeInterface& operator=(const RuntimeInterface&) = delete;
    
    VmManager* m_vmManager = nullptr;
    PeriphManager* m_periphManager = nullptr;
    std::vector<StatusCallback> m_callbacks;
    bool m_initialized = false;
    
    // 辅助函数
    void notifyStatusChange(const std::string& event, int vmId, const std::string& details);
    std::vector<std::string> splitCommand(const std::string& cmd);
};

#endif // RUNTIME_INTERFACE_H
