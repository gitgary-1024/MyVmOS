// 必须先包含 baseVM.h，因为 RuntimeInterface.h 通过 VmManager.h 间接需要它
#include "../vm/baseVM.h"
#include "../utils/RuntimeInterface.h"
#include <sstream>
#include <algorithm>
#include <chrono>
#include <iomanip>

// ==================== 初始化 ====================

void RuntimeInterface::initialize(VmManager* vmMgr, PeriphManager* periphMgr) {
    if (m_initialized) {
        return;
    }
    
    m_vmManager = vmMgr;
    m_periphManager = periphMgr;
    m_initialized = true;
    
    notifyStatusChange("SYSTEM_INIT", -1, "Runtime interface initialized");
}

void RuntimeInterface::shutdown() {
    if (!m_initialized) {
        return;
    }
    
    notifyStatusChange("SYSTEM_SHUTDOWN", -1, "Runtime interface shutting down");
    
    m_vmManager = nullptr;
    m_periphManager = nullptr;
    m_initialized = false;
}

// ==================== VM 管理接口 ====================

int RuntimeInterface::createVM(const std::string& type, const std::map<std::string, std::string>& config) {
    if (!m_initialized || !m_vmManager) {
        return -1;
    }
    
    try {
        // TODO: 根据 type 创建不同类型的 VM
        // 目前只支持 X86VM
        int vmId = m_vmManager->createX86VM();
        
        if (vmId >= 0) {
            notifyStatusChange("VM_CREATED", vmId, "Type: " + type);
        }
        
        return vmId;
    } catch (const std::exception& e) {
        notifyStatusChange("VM_CREATE_FAILED", -1, std::string("Error: ") + e.what());
        return -1;
    }
}

bool RuntimeInterface::destroyVM(int vmId) {
    if (!m_initialized || !m_vmManager) {
        return false;
    }
    
    // 参数验证
    if (vmId < 0) {
        notifyStatusChange("VM_DESTROY_FAILED", vmId, "Invalid VM ID: must be >= 0");
        return false;
    }
    
    // 检查 VM 是否存在
    auto vm = m_vmManager->get_vm(vmId);
    if (!vm) {
        notifyStatusChange("VM_DESTROY_FAILED", vmId, "VM does not exist");
        return false;
    }
    
    // 检查 VM 状态（不能销毁正在运行的 VM）
    VMState state = vm->get_state();
    if (state == VMState::RUNNING) {
        notifyStatusChange("VM_DESTROY_FAILED", vmId, "Cannot destroy running VM, stop it first");
        return false;
    }
    
    bool success = m_vmManager->destroyVM(vmId);
    
    if (success) {
        notifyStatusChange("VM_DESTROYED", vmId, "VM destroyed successfully");
    } else {
        notifyStatusChange("VM_DESTROY_FAILED", vmId, "Failed to destroy VM");
    }
    
    return success;
}

RuntimeInterface::VMStatus RuntimeInterface::getVMStatus(int vmId) {
    VMStatus status = {};
    
    if (!m_initialized || !m_vmManager) {
        status.id = vmId;
        status.state = "ERROR";
        status.error_message = "Runtime interface not initialized";
        return status;
    }
    
    // 检查 VM ID 有效性
    if (vmId < 0) {
        status.id = vmId;
        status.state = "ERROR";
        status.error_message = "Invalid VM ID: must be >= 0";
        return status;
    }
    
    // 获取 VM 对象
    auto vm = m_vmManager->get_vm(vmId);
    if (!vm) {
        status.id = vmId;
        status.state = "NOT_FOUND";
        status.error_message = "VM does not exist";
        return status;
    }
    
    // 填充真实状态信息
    status.id = vmId;
    
    // VM 类型（目前只有 X86）
    status.type = "X86";
    
    // VM 状态
    VMState state = vm->get_state();
    switch (state) {
        case VMState::CREATED:
            status.state = "CREATED";
            break;
        case VMState::RUNNING:
            status.state = "RUNNING";
            break;
        case VMState::SUSPENDED_WAITING_INTERRUPT:
            status.state = "WAITING_INTERRUPT";
            break;
        case VMState::TERMINATED:
            status.state = "TERMINATED";
            break;
        default:
            status.state = "UNKNOWN";
            break;
    }
    
    // 统计信息
    const VMScheduleCtx& ctx = vm->get_sched_ctx();
    status.instructions = ctx.instructions_executed;
    status.cycles = 0;  // 如果有周期计数器可以在这里设置
    
    // 内存使用（如果有内存信息）
    // status.memory_used = vm->get_memory_size();  // 假设 baseVM 有这个方法
    status.memory_used = 0;  // 暂时设为 0
    
    // 外设数量（可以通过 RouterCore 查询）
    status.peripheral_count = 0;  // 暂时设为 0
    
    // 调度信息
    status.priority = ctx.priority;
    status.has_quota = ctx.has_quota;
    status.blocked_reason = ctx.blocked_reason;
    
    return status;
}

std::vector<RuntimeInterface::VMStatus> RuntimeInterface::getAllVMsStatus() {
    std::vector<VMStatus> statuses;
    
    if (!m_initialized || !m_vmManager) {
        return statuses;
    }
    
    // TODO: 获取所有 VM 状态
    // 目前返回空列表
    
    return statuses;
}

bool RuntimeInterface::startVM(int vmId) {
    if (!m_initialized || !m_vmManager) {
        return false;
    }
    
    // TODO: 实现启动逻辑
    notifyStatusChange("VM_STARTED", vmId, "VM started");
    return true;
}

bool RuntimeInterface::stopVM(int vmId) {
    if (!m_initialized || !m_vmManager) {
        return false;
    }
    
    // 参数验证
    if (vmId < 0) {
        notifyStatusChange("VM_STOP_FAILED", vmId, "Invalid VM ID: must be >= 0");
        return false;
    }
    
    // 检查 VM 是否存在
    auto vm = m_vmManager->get_vm(vmId);
    if (!vm) {
        notifyStatusChange("VM_STOP_FAILED", vmId, "VM does not exist");
        return false;
    }
    
    // 检查 VM 状态（只能停止运行中的 VM）
    VMState state = vm->get_state();
    if (state != VMState::RUNNING) {
        std::string errorMsg = "VM is not running (current state: ";
        switch (state) {
            case VMState::CREATED:
                errorMsg += "CREATED";
                break;
            case VMState::SUSPENDED_WAITING_INTERRUPT:
                errorMsg += "WAITING_INTERRUPT";
                break;
            case VMState::TERMINATED:
                errorMsg += "TERMINATED";
                break;
            default:
                errorMsg += "UNKNOWN";
                break;
        }
        errorMsg += ")";
        notifyStatusChange("VM_STOP_FAILED", vmId, errorMsg);
        return false;
    }
    
    // 调用 VM 的 stop 方法
    vm->stop();
    
    notifyStatusChange("VM_STOPPED", vmId, "VM stopped successfully");
    return true;
}

bool RuntimeInterface::pauseVM(int vmId) {
    if (!m_initialized || !m_vmManager) {
        return false;
    }
    
    // TODO: 实现暂停逻辑
    notifyStatusChange("VM_PAUSED", vmId, "VM paused");
    return true;
}

bool RuntimeInterface::resumeVM(int vmId) {
    if (!m_initialized || !m_vmManager) {
        return false;
    }
    
    // TODO: 实现恢复逻辑
    notifyStatusChange("VM_RESUMED", vmId, "VM resumed");
    return true;
}

uint32_t RuntimeInterface::readRegister(int vmId, const std::string& regName) {
    if (!m_initialized || !m_vmManager) {
        return 0;
    }
    
    // TODO: 从 VM 读取寄存器
    // 示例：return m_vmManager->readRegister(vmId, regName);
    
    return 0;
}

void RuntimeInterface::writeRegister(int vmId, const std::string& regName, uint32_t value) {
    if (!m_initialized || !m_vmManager) {
        return;
    }
    
    // TODO: 写入 VM 寄存器
    // m_vmManager->writeRegister(vmId, regName, value);
    
    notifyStatusChange("VM_REG_WRITE", vmId, regName + " = " + std::to_string(value));
}

void RuntimeInterface::readMemory(int vmId, void* dest, size_t offset, size_t size) {
    if (!m_initialized || !m_vmManager || !dest) {
        return;
    }
    
    // TODO: 读取 VM 内存
    // m_vmManager->readMemory(vmId, dest, offset, size);
}

void RuntimeInterface::writeMemory(int vmId, const void* src, size_t offset, size_t size) {
    if (!m_initialized || !m_vmManager || !src) {
        return;
    }
    
    // TODO: 写入 VM 内存
    // m_vmManager->writeMemory(vmId, src, offset, size);
    
    notifyStatusChange("VM_MEM_WRITE", vmId, 
        "Offset: " + std::to_string(offset) + ", Size: " + std::to_string(size));
}

// ==================== 外设管理接口 ====================

RuntimeInterface::PeriphStatus RuntimeInterface::getPeriphStatus(const std::string& name) {
    PeriphStatus status = {};
    
    if (!m_initialized || !m_periphManager) {
        status.state = "ERROR";
        return status;
    }
    
    // TODO: 从 PeriphManager 获取真实状态
    status.name = name;
    status.type = "Unknown";
    status.state = "UNKNOWN";
    status.data_size = 0;
    status.has_pending_op = false;
    
    return status;
}

std::vector<RuntimeInterface::PeriphStatus> RuntimeInterface::getAllPeriphsStatus() {
    std::vector<PeriphStatus> statuses;
    
    if (!m_initialized || !m_periphManager) {
        return statuses;
    }
    
    // TODO: 获取所有外设状态
    
    return statuses;
}

bool RuntimeInterface::resetPeriph(const std::string& name) {
    if (!m_initialized || !m_periphManager) {
        return false;
    }
    
    // TODO: 重置外设
    // m_periphManager->resetDevice(name);
    
    notifyStatusChange("PERIPH_RESET", -1, "Device: " + name);
    return true;
}

bool RuntimeInterface::configurePeriph(const std::string& name, const std::map<std::string, std::string>& config) {
    if (!m_initialized || !m_periphManager) {
        return false;
    }
    
    // TODO: 配置外设
    // m_periphManager->configureDevice(name, config);
    
    notifyStatusChange("PERIPH_CONFIGURED", -1, "Device: " + name);
    return true;
}

bool RuntimeInterface::readPeriphData(const std::string& name, void* buffer, size_t size) {
    if (!m_initialized || !m_periphManager || !buffer) {
        return false;
    }
    
    // TODO: 读取外设数据
    // return m_periphManager->readDeviceData(name, buffer, size);
    
    return false;
}

bool RuntimeInterface::writePeriphData(const std::string& name, const void* buffer, size_t size) {
    if (!m_initialized || !m_periphManager || !buffer) {
        return false;
    }
    
    // TODO: 写入外设数据
    // return m_periphManager->writeDeviceData(name, buffer, size);
    
    return false;
}

// ==================== 消息调试接口 ====================

bool RuntimeInterface::sendTestMessage(int fromVmId, int toVmId, MessageType type, const void* payload, size_t size) {
    if (!m_initialized || !m_vmManager) {
        return false;
    }
    
    // TODO: 发送测试消息
    // m_vmManager->sendMessage(fromVmId, toVmId, type, payload, size);
    
    notifyStatusChange("TEST_MESSAGE_SENT", fromVmId, 
        "To: " + std::to_string(toVmId) + ", Type: " + std::to_string((int)type));
    
    return true;
}

RuntimeInterface::MessageQueueStatus RuntimeInterface::getMessageQueueStatus() {
    MessageQueueStatus status = {};
    
    if (!m_initialized) {
        return status;
    }
    
    // TODO: 获取消息队列状态
    
    return status;
}

// ==================== 系统监控接口 ====================

RuntimeInterface::SystemStatus RuntimeInterface::getSystemStatus() {
    SystemStatus status = {};
    
    if (!m_initialized) {
        return status;
    }
    
    // TODO: 收集系统统计信息
    
    return status;
}

RuntimeInterface::PerformanceStats RuntimeInterface::getPerformanceStats() {
    PerformanceStats stats = {};
    
    if (!m_initialized) {
        return stats;
    }
    
    // TODO: 计算性能统计
    
    return stats;
}

// ==================== 回调函数注册 ====================

void RuntimeInterface::registerStatusCallback(StatusCallback callback) {
    m_callbacks.push_back(callback);
}

void RuntimeInterface::notifyStatusChange(const std::string& event, int vmId, const std::string& details) {
    for (auto& callback : m_callbacks) {
        try {
            callback(event, vmId, details);
        } catch (...) {
            // 忽略回调异常
        }
    }
}

// ==================== 命令行接口 ====================

std::vector<std::string> RuntimeInterface::splitCommand(const std::string& cmd) {
    std::vector<std::string> tokens;
    std::istringstream iss(cmd);
    std::string token;
    
    while (iss >> token) {
        tokens.push_back(token);
    }
    
    return tokens;
}

std::string RuntimeInterface::executeCommand(const std::string& command) {
    if (!m_initialized) {
        return "Error: Runtime interface not initialized\n";
    }
    
    auto tokens = splitCommand(command);
    if (tokens.empty()) {
        return "Error: Empty command\n";
    }
    
    std::ostringstream output;
    const std::string& cmd = tokens[0];
    
    // ==================== VM 命令 ====================
    if (cmd == "vm") {
        if (tokens.size() < 2) {
            output << "Usage: vm <list|create|destroy|start|stop|pause|resume|status>\n";
            return output.str();
        }
        
        const std::string& subcmd = tokens[1];
        
        if (subcmd == "list") {
            auto vms = getAllVMsStatus();
            if (vms.empty()) {
                output << "No VMs created yet.\n";
            } else {
                output << "VMs:\n";
                output << std::left << std::setw(8) << "ID" 
                      << std::setw(12) << "Type" 
                      << std::setw(12) << "State"
                      << std::setw(20) << "Instructions"
                      << std::setw(15) << "Memory\n";
                output << std::string(67, '-') << "\n";
                
                for (const auto& vm : vms) {
                    output << std::left << std::setw(8) << vm.id
                          << std::setw(12) << vm.type
                          << std::setw(12) << vm.state
                          << std::setw(20) << vm.instructions
                          << std::setw(15) << vm.memory_used << "\n";
                }
            }
        }
        else if (subcmd == "create") {
            if (tokens.size() < 3) {
                output << "Usage: vm create <type>\n";
                output << "Supported types: X86\n";
                return output.str();
            }
            
            int vmId = createVM(tokens[2]);
            if (vmId >= 0) {
                output << "VM created successfully. ID: " << vmId << "\n";
            } else {
                output << "Failed to create VM\n";
            }
        }
        else if (subcmd == "destroy") {
            if (tokens.size() < 3) {
                output << "Usage: vm destroy <id>\n";
                return output.str();
            }
            
            int vmId = std::stoi(tokens[2]);
            if (destroyVM(vmId)) {
                output << "VM " << std::to_string(vmId) << " destroyed\n";
            } else {
                output << "Failed to destroy VM " << std::to_string(vmId) << "\n";
            }
        }
        else if (subcmd == "status") {
            if (tokens.size() < 3) {
                output << "Usage: vm status <id>\n";
                return output.str();
            }
            
            int vmId = std::stoi(tokens[2]);
            auto status = getVMStatus(vmId);
            output << "VM " << std::to_string(vmId) << " Status:\n";
            output << "  Type: " << status.type << "\n";
            output << "  State: " << status.state << "\n";
            output << "  Instructions: " << status.instructions << "\n";
            output << "  Cycles: " << status.cycles << "\n";
            output << "  Memory: " << status.memory_used << " bytes\n";
            output << "  Peripherals: " << status.peripheral_count << "\n";
        }
        else {
            output << "Unknown VM command: " << subcmd << "\n";
            output << "Available commands: list, create, destroy, start, stop, pause, resume, status\n";
        }
    }
    // ==================== 外设命令 ====================
    else if (cmd == "periph" || cmd == "device") {
        if (tokens.size() < 2) {
            output << "Usage: periph <list|show|reset|config>\n";
            return output.str();
        }
        
        const std::string& subcmd = tokens[1];
        
        if (subcmd == "list") {
            auto periphs = getAllPeriphsStatus();
            if (periphs.empty()) {
                output << "No peripherals found.\n";
            } else {
                output << "Peripherals:\n";
                output << std::left << std::setw(20) << "Name" 
                      << std::setw(15) << "Type" 
                      << std::setw(12) << "State"
                      << std::setw(15) << "Data Size"
                      << std::setw(10) << "Pending\n";
                output << std::string(72, '-') << "\n";
                
                for (const auto& p : periphs) {
                    output << std::left << std::setw(20) << p.name
                          << std::setw(15) << p.type
                          << std::setw(12) << p.state
                          << std::setw(15) << p.data_size
                          << std::setw(10) << (p.has_pending_op ? "Yes" : "No") << "\n";
                }
            }
        }
        else if (subcmd == "show") {
            if (tokens.size() < 3) {
                output << "Usage: periph show <name>\n";
                return output.str();
            }
            
            auto status = getPeriphStatus(tokens[2]);
            output << "Peripheral: " << status.name << "\n";
            output << "  Type: " << status.type << "\n";
            output << "  State: " << status.state << "\n";
            output << "  Data Size: " << status.data_size << " bytes\n";
            output << "  Pending Operations: " << (status.has_pending_op ? "Yes" : "No") << "\n";
        }
        else if (subcmd == "reset") {
            if (tokens.size() < 3) {
                output << "Usage: periph reset <name>\n";
                return output.str();
            }
            
            if (resetPeriph(tokens[2])) {
                output << "Peripheral " << tokens[2] << " reset successfully\n";
            } else {
                output << "Failed to reset peripheral " << tokens[2] << "\n";
            }
        }
        else {
            output << "Unknown peripheral command: " << subcmd << "\n";
            output << "Available commands: list, show, reset, config\n";
        }
    }
    // ==================== 系统命令 ====================
    else if (cmd == "system" || cmd == "sys") {
        if (tokens.size() < 2) {
            output << "Usage: system <status|stats>\n";
            return output.str();
        }
        
        const std::string& subcmd = tokens[1];
        
        if (subcmd == "status") {
            auto status = getSystemStatus();
            output << "System Status:\n";
            output << "  Total VMs: " << status.total_vms << "\n";
            output << "  Running VMs: " << status.running_vms << "\n";
            output << "  Total Peripherals: " << status.total_peripherals << "\n";
            output << "  Active Peripherals: " << status.active_peripherals << "\n";
            output << "  Total Instructions: " << status.total_instructions << "\n";
            output << "  Total Cycles: " << status.total_cycles << "\n";
            output << "  Memory Usage: " << status.memory_usage << " bytes\n";
            output << "  Uptime: " << std::fixed << std::setprecision(2) << status.uptime_seconds << " seconds\n";
        }
        else if (subcmd == "stats") {
            auto stats = getPerformanceStats();
            output << "Performance Statistics:\n";
            output << "  Avg VM CPU Usage: " << std::fixed << std::setprecision(2) << stats.avg_vm_cpu_usage << "%\n";
            output << "  Messages/sec: " << std::fixed << std::setprecision(2) << stats.messages_per_second << "\n";
            output << "  IO Ops/sec: " << std::fixed << std::setprecision(2) << stats.io_operations_per_second << "\n";
        }
        else {
            output << "Unknown system command: " << subcmd << "\n";
        }
    }
    // ==================== 帮助命令 ====================
    else if (cmd == "help" || cmd == "?") {
        output << "Available commands:\n";
        output << "  vm list                     - List all VMs\n";
        output << "  vm create <type>            - Create new VM\n";
        output << "  vm destroy <id>             - Destroy VM\n";
        output << "  vm status <id>              - Show VM status\n";
        output << "  vm start|stop|pause|resume  - Control VM\n";
        output << "  periph list                 - List all peripherals\n";
        output << "  periph show <name>          - Show peripheral details\n";
        output << "  periph reset <name>         - Reset peripheral\n";
        output << "  system status               - Show system status\n";
        output << "  system stats                - Show performance stats\n";
        output << "  help                        - Show this help\n";
    }
    else {
        output << "Unknown command: " << cmd << "\n";
        output << "Type 'help' for available commands.\n";
    }
    
    return output.str();
}
