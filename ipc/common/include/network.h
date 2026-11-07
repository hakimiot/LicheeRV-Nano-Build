#pragma once

#include <string>
#include <vector>
#include <nlohmann/json.hpp>

namespace NetworkInfo
{
    struct IpAddress {
        std::string address;     // IP 地址，如 "192.168.43.13" 或 "fe80::160a:2ff:fed9:7c95"
        int prefixLength = 0;    // 前缀长度，如 24
        std::string broadcast;   // IPv4 广播地址，IPv4 才有效
        std::string scope;       // 作用域，如 "global" / "link"
        bool isDynamic = false;  // 是否动态获取（IPv6 dynamic 标记）
    };

    struct InterfaceInfo {
        std::string name;                    // 网卡名，如 "wlan0"
        int index = 0;                       // 接口序号，如 6
        std::vector<std::string> flags;      // 标志位，如 BROADCAST, MULTICAST, UP, LOWER_UP
        int mtu = 0;                         // MTU，如 1500
        std::string state;                   // 状态，如 "UP"
        std::string macAddress;              // MAC 地址
        std::string broadcastMac;            // 广播 MAC
        std::vector<IpAddress> ipv4;         // IPv4 地址列表
        std::vector<IpAddress> ipv6;         // IPv6 地址列表
    };

    // 查询默认网卡名（通过 ip route show default 解析 dev 字段）
    // 若查询失败返回空字符串
    std::string GetDefaultInterface();

    // 查询所有网卡名（通过 ip -o link show 解析）
    std::vector<std::string> GetAllInterfaceNames();

    // 根据网卡名查询该网卡的完整信息（通过 ip addr show <iface> 解析）
    InterfaceInfo GetInterfaceInfo(const std::string& iface);

    // 查询所有网卡的完整信息
    std::vector<InterfaceInfo> GetAllInterfaces();

    // JSON 版本，便于 HTTP 接口直接返回
    nlohmann::json GetInterfaceInfoJson(const std::string& iface);
    nlohmann::json GetAllInterfacesJson();
}
