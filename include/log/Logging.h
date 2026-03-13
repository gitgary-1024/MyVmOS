#ifndef LOGGING_H
#define LOGGING_H

#include <iostream>
#include <string>
#include <fstream>
#include <sstream>
#include <mutex>
#include <memory>
#include <functional>

// ==========================================
// 1. 日志等级枚举（类似Python logging）
// ==========================================
enum class LogLevel {
    DEBUG = 10,
    INFO = 20,
    WARNING = 30,
    ERR = 40,      // 使用 ERR 而非 ERROR，避免与 Windows GDI 宏冲突
    CRITICAL = 50,
    NOTSET = 0
};

// ==========================================
// 2. 终端颜色宏（跨平台）
// ==========================================
#ifdef _WIN32
    #include <windows.h>
    
    // 在 Windows 下，先保存 ERROR 宏，然后 undefine 它
    #ifdef ERROR
        #define WINDOWS_ERROR_MACRO_WAS_DEFINED
        #pragma push_macro("ERROR")
        #undef ERROR
    #endif
    
    #define COLOR_RESET       ""
    #define COLOR_DEBUG       ""  // Windows 下通过 API 设置
    #define COLOR_INFO        ""
    #define COLOR_WARNING     ""
    #define COLOR_ERR         ""
    #define COLOR_CRITICAL    ""
#else
    #include <unistd.h>
    
    #define COLOR_RESET       "\033[0m"
    #define COLOR_DEBUG       "\033[36m"  // 青色
    #define COLOR_INFO        "\033[32m"  // 绿色
    #define COLOR_WARNING     "\033[33m"  // 黄色
    #define COLOR_ERR         "\033[31m"  // 红色
    #define COLOR_CRITICAL    "\033[35m"  // 紫色
#endif

// ==========================================
// 3. 日志记录器类（类似Python Logger）
// ==========================================
class Logger {
public:
    static Logger& instance();
    
    // 禁止拷贝和移动
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;
    
    // 设置日志等级
    void set_level(LogLevel level);
    LogLevel get_level() const { return level_; }
    
    // 设置输出流
    void set_output(std::ostream& os);
    void set_file_output(const std::string& filename);
    
    // 日志接口
    void debug(const std::string& msg, const std::string& module = "");
    void info(const std::string& msg, const std::string& module = "");
    void warning(const std::string& msg, const std::string& module = "");
    void err(const std::string& msg, const std::string& module = "");      // 使用 err 而非 error
    void critical(const std::string& msg, const std::string& module = "");
    
    // 格式化日志消息
    std::string format_message(LogLevel level, const std::string& msg, 
                               const std::string& module, const std::string& time_str);
    
    // 获取颜色字符串
    const char* get_color(LogLevel level);
    
private:
    Logger();
    ~Logger();
    
    void log(LogLevel level, const std::string& msg, const std::string& module);
    std::string get_time_string();
    std::string level_to_string(LogLevel level);
    
    LogLevel level_;
    std::ostream* output_stream_;
    std::unique_ptr<std::ofstream> file_stream_;
    std::mutex log_mutex_;
    bool use_colors_;
};

// ==========================================
// 4. 全局快捷宏（类似Python logging 模块级函数）
// ==========================================
#define LOG_DEBUG(msg)    Logger::instance().debug(msg, __FILE_NAME__)
#define LOG_INFO(msg)     Logger::instance().info(msg, __FILE_NAME__)
#define LOG_WARNING(msg)  Logger::instance().warning(msg, __FILE_NAME__)
#define LOG_ERR(msg)      Logger::instance().err(msg, __FILE_NAME__)        // 使用 LOG_ERR
#define LOG_CRITICAL(msg) Logger::instance().critical(msg, __FILE_NAME__)

// 带模块名的版本
#define LOG_DEBUG_MOD(module, msg)    Logger::instance().debug(msg, module)
#define LOG_INFO_MOD(module, msg)     Logger::instance().info(msg, module)
#define LOG_WARNING_MOD(module, msg)  Logger::instance().warning(msg, module)
#define LOG_ERR_MOD(module, msg)      Logger::instance().err(msg, module)
#define LOG_CRITICAL_MOD(module, msg) Logger::instance().critical(msg, module)


#endif // LOGGING_H
