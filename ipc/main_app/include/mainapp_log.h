#pragma once

#include <iostream>
#include <string>
#include <cstring>

// ==================== 日志等级定义 ====================
#define LOG_LEVEL_N  0
#define LOG_LEVEL_E 1
#define LOG_LEVEL_W  2
#define LOG_LEVEL_I  3
#define LOG_LEVEL_D 4

// 默认日志等级
#ifndef LOG_LEVEL
#define LOG_LEVEL LOG_LEVEL_I
#endif

// ==================== 条件编译宏 ====================
#if LOG_LEVEL >= LOG_LEVEL_D
#define LOG_ENABLE_DEBUG 1
#else
#define LOG_ENABLE_DEBUG 0
#endif

#if LOG_LEVEL >= LOG_LEVEL_I
#define LOG_ENABLE_INFO 1
#else
#define LOG_ENABLE_INFO 0
#endif

#if LOG_LEVEL >= LOG_LEVEL_W
#define LOG_ENABLE_WARN 1
#else
#define LOG_ENABLE_WARN 0
#endif

#if LOG_LEVEL >= LOG_LEVEL_E
#define LOG_ENABLE_ERROR 1
#else
#define LOG_ENABLE_ERROR 0
#endif

// ==================== 辅助宏 ====================
#define GET_FILE_NAME(path) \
    (strrchr(path, '/') ? strrchr(path, '/') + 1 : \
     strrchr(path, '\\') ? strrchr(path, '\\') + 1 : path)

// ==================== 日志宏 ====================
#if LOG_ENABLE_DEBUG
#define LOG_D \
    LogLine(std::cout, GetCurrentTime(), "Debug", \
            GET_FILE_NAME(__FILE__), __LINE__, __FUNCTION__)
#else
#define LOG_D if (false) LogLine(std::cout, GetCurrentTime(), "Debug", \
                   GET_FILE_NAME(__FILE__), __LINE__, __FUNCTION__)
#endif

#if LOG_ENABLE_INFO
#define LOG_I \
    LogLine(std::cout, GetCurrentTime(), "Info", \
            GET_FILE_NAME(__FILE__), __LINE__, __FUNCTION__)
#else
#define LOG_I if (false) LogLine(std::cout, GetCurrentTime(), "Info", \
                   GET_FILE_NAME(__FILE__), __LINE__, __FUNCTION__)
#endif

#if LOG_ENABLE_WARN
#define LOG_W \
    LogLine(std::cout, GetCurrentTime(), "Warn", \
            GET_FILE_NAME(__FILE__), __LINE__, __FUNCTION__)
#else
#define LOG_W if (false) LogLine(std::cout, GetCurrentTime(), "Warn", \
                   GET_FILE_NAME(__FILE__), __LINE__, __FUNCTION__)
#endif

#if LOG_ENABLE_ERROR
#define LOG_E \
    LogLine(std::cout, GetCurrentTime(), "Error", \
            GET_FILE_NAME(__FILE__), __LINE__, __FUNCTION__)
#else
#define LOG_E if (false) LogLine(std::cout, GetCurrentTime(), "Error", \
                   GET_FILE_NAME(__FILE__), __LINE__, __FUNCTION__)
#endif

// ==================== LogLine 类 ====================
class LogLine {
public:
    LogLine(std::ostream& os, const std::string& time, const std::string& level,
            const char* file, int line, const char* func)
        : m_os(os) {
        m_os << "[" << time << "][" << level << "]"
             << "[" << file << ":" << line << "]"
             << "[" << func << "] ";
    }
    
    ~LogLine() { m_os << std::endl; }
    
    template<typename T>
    LogLine& operator<<(const T& val) {
        m_os << val;
        return *this;
    }
    
private:
    std::ostream& m_os;
};

std::string GetCurrentTime();
