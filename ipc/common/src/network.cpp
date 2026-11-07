#include "network.h"

#include <cstdio>
#include <cstring>
#include <sstream>
#include <string>
#include <vector>

namespace {

// 去除字符串首尾空白
std::string Trim(const std::string& s)
{
    size_t start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    size_t end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}

// 按空白字符分割字符串为 token 列表
std::vector<std::string> SplitTokens(const std::string& s)
{
    std::vector<std::string> tokens;
    std::istringstream iss(s);
    std::string tok;
    while (iss >> tok) {
        tokens.push_back(tok);
    }
    return tokens;
}

// 执行命令并返回输出
std::string ExecCommand(const std::string& cmd)
{
    std::string output;
    FILE* fp = popen(cmd.c_str(), "r");
    if (!fp) return output;
    char buf[1024];
    while (fgets(buf, sizeof(buf), fp)) {
        output += buf;
    }
    pclose(fp);
    return output;
}

} // namespace

std::string NetworkInfo::GetDefaultInterface()
{
    // ip route show default 输出形如：
    // default via 192.168.43.1 dev wlan0 proto dhcp src 192.168.43.13 metric 1024
    std::string output = ExecCommand("ip route show default");
    if (output.empty()) return "";

    std::istringstream iss(output);
    std::string line;
    while (std::getline(iss, line)) {
        auto tokens = SplitTokens(line);
        // 找到 "dev" 关键字，其后的 token 即为默认网卡
        for (size_t i = 0; i + 1 < tokens.size(); ++i) {
            if (tokens[i] == "dev") {
                return tokens[i + 1];
            }
        }
    }
    return "";
}

NetworkInfo::InterfaceInfo NetworkInfo::GetInterfaceInfo(const std::string& iface)
{
    InterfaceInfo info;
    if (iface.empty()) return info;

    info.name = iface;
    std::string output = ExecCommand("ip addr show " + iface);
    if (output.empty()) return info;

    std::istringstream iss(output);
    std::string line;
    bool firstLineParsed = false;

    while (std::getline(iss, line)) {
        std::string trimmed = Trim(line);
        if (trimmed.empty()) continue;

        auto tokens = SplitTokens(trimmed);
        if (tokens.empty()) continue;

        // 第一行：6: wlan0: <BROADCAST,MULTICAST,UP,LOWER_UP> mtu 1500 qdisc mq state UP group default qlen 1000
        if (!firstLineParsed) {
            firstLineParsed = true;
            // 解析 index 和 name
            // tokens[0] = "6:", tokens[1] = "wlan0:"
            if (tokens.size() >= 2) {
                std::string idxStr = tokens[0];
                if (!idxStr.empty() && idxStr.back() == ':') idxStr.pop_back();
                try { info.index = std::stoi(idxStr); } catch (...) {}
            }

            // 解析 flags <...>
            size_t lt = trimmed.find('<');
            size_t gt = trimmed.find('>');
            if (lt != std::string::npos && gt != std::string::npos && gt > lt) {
                std::string flagStr = trimmed.substr(lt + 1, gt - lt - 1);
                std::string flag;
                std::istringstream fs(flagStr);
                while (std::getline(fs, flag, ',')) {
                    flag = Trim(flag);
                    if (!flag.empty()) info.flags.push_back(flag);
                }
            }

            // 解析 mtu 和 state
            for (size_t i = 0; i < tokens.size(); ++i) {
                if (tokens[i] == "mtu" && i + 1 < tokens.size()) {
                    try { info.mtu = std::stoi(tokens[i + 1]); } catch (...) {}
                } else if (tokens[i] == "state" && i + 1 < tokens.size()) {
                    info.state = tokens[i + 1];
                }
            }
            continue;
        }

        // link/ether 行
        if (tokens[0] == "link/ether") {
            if (tokens.size() >= 2) info.macAddress = tokens[1];
            for (size_t i = 1; i + 1 < tokens.size(); ++i) {
                if (tokens[i] == "brd") {
                    info.broadcastMac = tokens[i + 1];
                    break;
                }
            }
            continue;
        }

        // inet 行：inet 192.168.43.13/24 brd 192.168.43.255 scope global wlan0
        if (tokens[0] == "inet") {
            IpAddress ip;
            if (tokens.size() >= 2) {
                std::string addrWithPrefix = tokens[1];
                size_t slash = addrWithPrefix.find('/');
                if (slash != std::string::npos) {
                    ip.address = addrWithPrefix.substr(0, slash);
                    try { ip.prefixLength = std::stoi(addrWithPrefix.substr(slash + 1)); } catch (...) {}
                } else {
                    ip.address = addrWithPrefix;
                }
            }
            for (size_t i = 1; i < tokens.size(); ++i) {
                if (tokens[i] == "brd" && i + 1 < tokens.size()) {
                    ip.broadcast = tokens[i + 1];
                } else if (tokens[i] == "scope" && i + 1 < tokens.size()) {
                    ip.scope = tokens[i + 1];
                }
            }
            info.ipv4.push_back(ip);
            continue;
        }

        // inet6 行：inet6 240a:42d8:0:6504:.../64 scope global dynamic mngtmpaddr
        if (tokens[0] == "inet6") {
            IpAddress ip;
            if (tokens.size() >= 2) {
                std::string addrWithPrefix = tokens[1];
                size_t slash = addrWithPrefix.find('/');
                if (slash != std::string::npos) {
                    ip.address = addrWithPrefix.substr(0, slash);
                    try { ip.prefixLength = std::stoi(addrWithPrefix.substr(slash + 1)); } catch (...) {}
                } else {
                    ip.address = addrWithPrefix;
                }
            }
            for (size_t i = 1; i < tokens.size(); ++i) {
                if (tokens[i] == "scope" && i + 1 < tokens.size()) {
                    ip.scope = tokens[i + 1];
                } else if (tokens[i] == "dynamic") {
                    ip.isDynamic = true;
                }
            }
            info.ipv6.push_back(ip);
            continue;
        }
    }

    return info;
}

nlohmann::json NetworkInfo::GetInterfaceInfoJson(const std::string& iface)
{
    InterfaceInfo info = GetInterfaceInfo(iface);

    nlohmann::json flagsJson = nlohmann::json::array();
    for (const auto& f : info.flags) flagsJson.push_back(f);

    nlohmann::json ipv4Json = nlohmann::json::array();
    for (const auto& ip : info.ipv4) {
        ipv4Json.push_back({
            {"address", ip.address},
            {"prefixLength", ip.prefixLength},
            {"broadcast", ip.broadcast},
            {"scope", ip.scope}
        });
    }

    nlohmann::json ipv6Json = nlohmann::json::array();
    for (const auto& ip : info.ipv6) {
        ipv6Json.push_back({
            {"address", ip.address},
            {"prefixLength", ip.prefixLength},
            {"scope", ip.scope},
            {"dynamic", ip.isDynamic}
        });
    }

    return {
        {"name", info.name},
        {"index", info.index},
        {"flags", flagsJson},
        {"mtu", info.mtu},
        {"state", info.state},
        {"macAddress", info.macAddress},
        {"broadcastMac", info.broadcastMac},
        {"ipv4", ipv4Json},
        {"ipv6", ipv6Json}
    };
}

std::vector<std::string> NetworkInfo::GetAllInterfaceNames()
{
    std::vector<std::string> names;
    // ip -o link show 每行一个网卡，格式如：
    // 6: wlan0: <BROADCAST,...> mtu 1500 ...
    std::string output = ExecCommand("ip -o link show");
    if (output.empty()) return names;

    std::istringstream iss(output);
    std::string line;
    while (std::getline(iss, line)) {
        // 提取 "N: ifname:" 中的 ifname
        size_t firstColon = line.find(':');
        if (firstColon == std::string::npos) continue;
        size_t secondColon = line.find(':', firstColon + 1);
        if (secondColon == std::string::npos) continue;

        std::string name = line.substr(firstColon + 1, secondColon - firstColon - 1);
        name = Trim(name);
        if (!name.empty()) {
            names.push_back(name);
        }
    }
    return names;
}

std::vector<NetworkInfo::InterfaceInfo> NetworkInfo::GetAllInterfaces()
{
    std::vector<InterfaceInfo> result;
    std::vector<std::string> names = GetAllInterfaceNames();
    for (const auto& name : names) {
        result.push_back(GetInterfaceInfo(name));
    }
    return result;
}

nlohmann::json NetworkInfo::GetAllInterfacesJson()
{
    nlohmann::json arr = nlohmann::json::array();
    std::vector<std::string> names = GetAllInterfaceNames();
    for (const auto& name : names) {
        arr.push_back(GetInterfaceInfoJson(name));
    }
    return arr;
}
