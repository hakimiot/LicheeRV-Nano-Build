#pragma once

#include <string>

#define SIZE_1KB (1024LL)
#define SIZE_1MB (1024LL * 1024LL)

namespace DeviceInfo
{
    struct DeviceInfo_t {
        std::string key;
        std::string name;
        std::string version;
    };

    // total/used/free/available: bytes
    struct MemoryInfo {
        long total = 0;
        long used = 0;
        long free = 0;
        long available = 0;
    };

    // total/used/free/available: bytes
    struct DiskInfo {
        long long total = 0;
        long long used = 0;
        long long free = 0;
        long long available = 0;
    };

    struct CpuInfo {
        double load = 0;
        long long bootTime = 0;
        double temperature = 0;
    };

    std::string DeviceKey();
    DeviceInfo_t DeviceInfo();

    bool ReadMemInfo(MemoryInfo &info, std::string &error);
    bool ReadCpuLoad(double &load, std::string &error);
    bool ReadUptime(long long &bootTime, std::string &error);
    bool ReadTemperature(double &temperature, std::string &error);

    bool GetMemoryInfo(MemoryInfo &info, std::string &error);
    bool GetDiskInfo(const char *path, DiskInfo &info, std::string &error);
    bool GetCpuInfo(CpuInfo &info, std::string &error);
};
