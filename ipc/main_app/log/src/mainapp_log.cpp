#include <chrono>
#include <iomanip>
#include <sstream>
#include <ctime>
#include <cstring>
#include "mainapp_log.h"

std::string GetCurrentTime()
{
    auto now = std::chrono::system_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;
    auto time_t_now = std::chrono::system_clock::to_time_t(now);
    
    std::stringstream ss;
    ss << std::put_time(std::localtime(&time_t_now), "%Y-%m-%d %H:%M:%S");
    ss << '.' << std::setfill('0') << std::setw(3) << ms.count();
    return ss.str();
}

const char* GetFileName(const char* path)
{
    const char* last_slash = strrchr(path, '/');
    return last_slash ? last_slash + 1 : path;
}
