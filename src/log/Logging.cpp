#include "log/Logging.h"
#include <iostream>
#include <string>
#include <fstream>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <chrono>

#ifdef _WIN32
    #include <windows.h>
#endif

// ==========================================
// Logger 单例实现
// ==========================================
Logger& Logger::instance() {
    static Logger instance;
    return instance;
}

Logger::Logger() 
    : level_(LogLevel::DEBUG)
    , output_stream_(&std::cout)
    , file_stream_(nullptr)
    , use_colors_(true) {
    // 检测是否支持颜色（Linux/Mac 默认支持，Windows 10+ 支持）
#ifdef _WIN32
    // Windows 10 及以上版本支持 ANSI 颜色
    use_colors_ = true;  // 简化处理，默认启用
#else
    use_colors_ = isatty(fileno(stdout));
#endif
}

Logger::~Logger() {
    if (file_stream_ && file_stream_->is_open()) {
        file_stream_->close();
    }
}

void Logger::set_level(LogLevel level) {
    std::lock_guard<std::mutex> lock(log_mutex_);
    level_ = level;
}

void Logger::set_output(std::ostream& os) {
    std::lock_guard<std::mutex> lock(log_mutex_);
    output_stream_ = &os;
    file_stream_.reset();  // 清除文件输出
}

void Logger::set_file_output(const std::string& filename) {
    std::lock_guard<std::mutex> lock(log_mutex_);
    file_stream_ = std::make_unique<std::ofstream>(filename, std::ios::app);
    if (file_stream_->is_open()) {
        output_stream_ = file_stream_.get();
        use_colors_ = false;  // 文件输出不使用颜色
    }
}

const char* Logger::get_color(LogLevel level) {
#ifdef _WIN32
    // Windows 控制台颜色设置
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    switch (level) {
        case LogLevel::DEBUG:    SetConsoleTextAttribute(hConsole, FOREGROUND_BLUE | FOREGROUND_GREEN); break;     // 青色
        case LogLevel::INFO:     SetConsoleTextAttribute(hConsole, FOREGROUND_GREEN); break;  // 绿色
        case LogLevel::WARNING:  SetConsoleTextAttribute(hConsole, FOREGROUND_RED | FOREGROUND_GREEN); break; // 黄色
        case LogLevel::ERR:      SetConsoleTextAttribute(hConsole, FOREGROUND_RED); break;    // 红色
        case LogLevel::CRITICAL: SetConsoleTextAttribute(hConsole, FOREGROUND_RED | FOREGROUND_BLUE); break; // 紫色
        default:                 SetConsoleTextAttribute(hConsole, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE); break;
    }
    return "";
#else
    // Linux/Mac ANSI 颜色
    switch (level) {
        case LogLevel::DEBUG:    return COLOR_DEBUG;
        case LogLevel::INFO:     return COLOR_INFO;
        case LogLevel::WARNING:  return COLOR_WARNING;
        case LogLevel::ERR:      return COLOR_ERR;
        case LogLevel::CRITICAL: return COLOR_CRITICAL;
        default:                 return COLOR_RESET;
    }
#endif
}

void Logger::log(LogLevel level, const std::string& msg, const std::string& module) {
    // 检查日志等级
    if (level < level_) {
        return;
    }
    
    std::lock_guard<std::mutex> lock(log_mutex_);
    
    // 获取时间戳
    std::string time_str = get_time_string();
    
    // 提取文件名（去掉路径）
    std::string short_module = module;
    size_t pos = module.find_last_of("/\\");
    if (pos != std::string::npos) {
        short_module = module.substr(pos + 1);
    }
    
    // 格式化并输出
    std::string formatted = format_message(level, msg, short_module, time_str);
    *output_stream_ << formatted << std::endl;
    output_stream_->flush();
    
#ifdef _WIN32
    // Windows 下重置颜色
    if (use_colors_) {
        HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
        SetConsoleTextAttribute(hConsole, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
    }
#endif
}

std::string Logger::format_message(LogLevel level, const std::string& msg, 
                                   const std::string& module, const std::string& time_str) {
    std::ostringstream oss;
    
    // 添加颜色
    if (use_colors_) {
        oss << get_color(level);
    }
    
    // 格式：[时间] [等级] [模块] 消息
    oss << "[" << time_str << "] "
        << "[" << std::setw(8) << level_to_string(level) << "] ";
    
    if (!module.empty()) {
        oss << "[" << module << "] ";
    }
    
    oss << msg;
    
    // 重置颜色
    if (use_colors_) {
        oss << COLOR_RESET;
    }
    
    return oss.str();
}

std::string Logger::get_time_string() {
    auto now = std::chrono::system_clock::now();
    auto time_t_now = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;
    
    std::ostringstream oss;
    oss << std::put_time(std::localtime(&time_t_now), "%Y-%m-%d %H:%M:%S")
        << '.' << std::setfill('0') << std::setw(3) << ms.count();
    
    return oss.str();
}

std::string Logger::level_to_string(LogLevel level) {
    switch (level) {
        case LogLevel::DEBUG:    return "DEBUG";
        case LogLevel::INFO:     return "INFO";
        case LogLevel::WARNING:  return "WARNING";
        case LogLevel::ERR:      return "ERROR";   // 显示时仍用 ERROR，但枚举值用 ERR
        case LogLevel::CRITICAL: return "CRITICAL";
        default:                 return "NOTSET";
    }
}

// ==========================================
// 日志接口实现
// ==========================================
void Logger::debug(const std::string& msg, const std::string& module) {
    log(LogLevel::DEBUG, msg, module);
}

void Logger::info(const std::string& msg, const std::string& module) {
    log(LogLevel::INFO, msg, module);
}

void Logger::warning(const std::string& msg, const std::string& module) {
    log(LogLevel::WARNING, msg, module);
}

void Logger::err(const std::string& msg, const std::string& module) {
    log(LogLevel::ERR, msg, module);
}

void Logger::critical(const std::string& msg, const std::string& module) {
    log(LogLevel::CRITICAL, msg, module);
}


// ==========================================
// Windows ERROR 宏恢复
// ==========================================
#ifdef _WIN32
    #ifdef WINDOWS_ERROR_MACRO_WAS_DEFINED
        #pragma pop_macro("ERROR")
        #undef WINDOWS_ERROR_MACRO_WAS_DEFINED
    #endif
#endif
