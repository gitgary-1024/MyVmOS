#include "log/Logging.h"
#include <thread>
#include <chrono>

int main() {
    auto& logger = Logger::instance();
    
    // 设置日志等级为 DEBUG（显示所有日志）
    logger.set_level(LogLevel::DEBUG);
    
    std::cout << "\n=== Testing Colored Logging System ===\n\n";
    
    // 测试各个等级的日志
    LOG_DEBUG("This is a DEBUG message - cyan color");
    LOG_INFO("This is an INFO message - green color");
    LOG_WARNING("This is a WARNING message - yellow color");
    LOG_ERR("This is an ERROR message - red color");
    LOG_CRITICAL("This is a CRITICAL message - purple color");
    
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // 带模块名的日志
    LOG_INFO_MOD("Router", "Router initialized");
    LOG_DEBUG_MOD("VM", "VM created with ID=1");
    LOG_WARNING_MOD("Disk", "Disk space low");
    LOG_ERR_MOD("NIC", "Network connection failed");
    LOG_CRITICAL_MOD("System", "Critical system failure");
    
    // 测试日志等级过滤
    std::cout << "\n--- Changing log level to WARNING ---\n\n";
    logger.set_level(LogLevel::WARNING);
    
    LOG_DEBUG("This DEBUG should be HIDDEN");
    LOG_INFO("This INFO should be HIDDEN");
    LOG_WARNING("This WARNING should be VISIBLE");
    LOG_ERR("This ERROR should be VISIBLE");
    LOG_CRITICAL("This CRITICAL should be VISIBLE");
    
    std::cout << "\n=== Test completed successfully! ===\n";
    
    return 0;
}
