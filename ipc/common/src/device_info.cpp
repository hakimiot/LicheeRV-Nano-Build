#include <fstream>
#include <sstream>
#include <chrono>
#include <dirent.h>
#include <exception>
#include <iterator>
#include <set>
#include <string>
#include <vector>
#include <algorithm>
#include <sys/stat.h>
#include <unistd.h>
#include <thread>
#include <utility>
#include <sys/statvfs.h>
#include <cstdio>
#include <cerrno>
#include <cstring>
#include "file_system.h"
#include "device_info.h"

namespace {

struct CpuTimes {
    unsigned long long total = 0;
    unsigned long long idle = 0;
};

void AppendError(std::string &error, const std::string &message)
{
    if (message.empty()) {
        return;
    }
    if (!error.empty()) {
        error += "; ";
    }
    error += message;
}

bool ReadFile(const char *path, std::string &text, std::string &error)
{
    std::ifstream file(path);
    if (!file.is_open()) {
        error = std::string("Failed to open ") + path;
        return false;
    }

    text.assign(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
    if (file.bad()) {
        error = std::string("Failed to read ") + path;
        return false;
    }
    return true;
}

bool ParseLong(const std::string &text, long &value, std::string &error, const char *what)
{
    try {
        size_t pos = 0;
        value = std::stol(text, &pos);
        if (pos == 0) {
            error = std::string(what) + " is invalid";
            return false;
        }
        return true;
    } catch (const std::exception &) {
        error = std::string(what) + " is invalid";
        return false;
    }
}

bool ReadMemInfoValue(const std::string &text, const char *key, long &value, std::string &error)
{
    const std::string pattern = std::string(key) + ":";
    const size_t pos = text.find(pattern);
    if (pos == std::string::npos) {
        error = std::string(key) + " not found";
        return false;
    }
    return ParseLong(text.substr(pos + pattern.size()), value, error, key);
}

bool ReadCpuTimes(CpuTimes &times, std::string &error)
{
    std::string text;
    if (!ReadFile("/proc/stat", text, error)) {
        return false;
    }

    const size_t lineEnd = text.find('\n');
    const std::string line = text.substr(0, lineEnd);
    unsigned long long user = 0;
    unsigned long long nice = 0;
    unsigned long long system = 0;
    unsigned long long idle = 0;
    unsigned long long iowait = 0;
    unsigned long long irq = 0;
    unsigned long long softirq = 0;
    unsigned long long steal = 0;
    if (std::sscanf(line.c_str(), "cpu %llu %llu %llu %llu %llu %llu %llu %llu",
            &user, &nice, &system, &idle, &iowait, &irq, &softirq, &steal) < 4) {
        error = "Failed to parse /proc/stat";
        return false;
    }

    times.idle = idle + iowait;
    times.total = user + nice + system + times.idle + irq + softirq + steal;
    return true;
}
}

namespace DeviceInfo {
std::string DeviceKey()
{
    std::string id = FileSystem::ReadFile("/device_key");
    if (id.empty()) {
        return "default123456";
    } else {
        id.pop_back();
    }
    return id;
}

DeviceInfo_t DeviceInfo()
{
    DeviceInfo_t info;
    std::string text = FileSystem::ReadFile("/boot/version");
    if (!text.empty()) {
        std::istringstream iss(text);
        std::string line;
        while (std::getline(iss, line)) {
            // 按第一个 '=' 分割为 key 和 value
            size_t eqPos = line.find('=');
            if (eqPos == std::string::npos) {
                continue;
            }
            std::string key = line.substr(0, eqPos);
            std::string value = line.substr(eqPos + 1);

            // 去除 value 首尾空白字符（含 \r、\n、空格、制表符）
            size_t start = value.find_first_not_of(" \t\r\n");
            size_t end = value.find_last_not_of(" \t\r\n");
            if (start != std::string::npos) {
                value = value.substr(start, end - start + 1);
            } else {
                value.clear();
            }

            if (key == "NAME") {
                info.name = value;
            } else if (key == "VERSION") {
                info.version = value;
            }
        }
    }

    info.key = DeviceKey();

    return info;
}

bool ReadMemInfo(MemoryInfo &info, std::string &error)
{
    std::string text;
    if (!ReadFile("/proc/meminfo", text, error)) {
        return false;
    }
    if (!ReadMemInfoValue(text, "MemTotal", info.total, error) ||
        !ReadMemInfoValue(text, "MemFree", info.free, error) ||
        !ReadMemInfoValue(text, "MemAvailable", info.available, error)) {
        return false;
    }
    // /proc/meminfo values are in kB; expose as bytes.
    info.total *= SIZE_1KB;
    info.free *= SIZE_1KB;
    info.available *= SIZE_1KB;
    info.used = info.total - info.free;
    return true;
}

bool ReadCpuLoad(double &load, std::string &error)
{
    CpuTimes begin;
    CpuTimes end;
    if (!ReadCpuTimes(begin, error)) {
        return false;
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    if (!ReadCpuTimes(end, error)) {
        return false;
    }
    if (end.total <= begin.total) {
        error = "CPU stat did not advance";
        return false;
    }

    unsigned long long total = end.total - begin.total;
    unsigned long long idle = end.idle > begin.idle ? end.idle - begin.idle : 0;
    if (idle > total) {
        idle = total;
    }
    load = 100.0 * static_cast<double>(total - idle) / static_cast<double>(total);
    return true;
}

bool ReadUptime(long long &bootTime, std::string &error)
{
    std::string text;
    if (!ReadFile("/proc/uptime", text, error)) {
        return false;
    }

    try {
        size_t pos = 0;
        bootTime = std::stoll(text, &pos);
        if (pos == 0) {
            error = "Failed to parse /proc/uptime";
            return false;
        }
        return true;
    } catch (const std::exception &) {
        error = "Failed to parse /proc/uptime";
        return false;
    }
}

bool ReadTemperature(double &temperature, std::string &error)
{
    const char *path = "/sys/class/thermal/thermal_zone0/temp";
    std::string text;
    if (!ReadFile(path, text, error)) {
        return false;
    }

    long raw = 0;
    if (!ParseLong(text, raw, error, path)) {
        return false;
    }
    temperature = static_cast<double>(raw) / 1000.0;
    return true;
}

bool GetMemoryInfo(MemoryInfo &info, std::string &error)
{
    info = MemoryInfo{};
    error.clear();
    return ReadMemInfo(info, error);
}

bool GetDiskInfo(const char *path, DiskInfo &info, std::string &error)
{
    info = DiskInfo{};
    error.clear();
    if (path == nullptr || path[0] == '\0') {
        error = "Disk path is empty";
        return false;
    }

    struct statvfs vfs;
    if (statvfs(path, &vfs) != 0) {
        error = std::string("statvfs(") + path + ") failed: " + std::strerror(errno);
        return false;
    }

    unsigned long long blockSize = vfs.f_frsize;
    info.total = static_cast<long long>((unsigned long long)vfs.f_blocks * blockSize);
    info.free = static_cast<long long>((unsigned long long)vfs.f_bfree * blockSize);
    info.available = static_cast<long long>((unsigned long long)vfs.f_bavail * blockSize);
    info.used = info.total - info.free;

    return true;
}

bool GetCpuInfo(CpuInfo &info, std::string &error)
{
    info = CpuInfo{};
    error.clear();

    std::string part;
    if (!ReadCpuLoad(info.load, part)) {
        AppendError(error, part);
    }
    part.clear();
    if (!ReadUptime(info.bootTime, part)) {
        AppendError(error, part);
    }
    part.clear();
    if (!ReadTemperature(info.temperature, part)) {
        AppendError(error, part);
    }
    return error.empty();
}
}
