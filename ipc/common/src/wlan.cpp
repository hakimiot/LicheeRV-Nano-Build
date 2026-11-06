#include <cstdio>
#include <iostream>
#include <sstream>
#include <fstream>
#include <thread>
#include <chrono>
#include <set>
#include "wlan.h"

// 将 iw 输出的转义字符串 (如 \xe7\x9a\x84) 转换为普通 UTF-8 字符串
std::string UnescapeSsid(const std::string& src)
{
    std::string dst;
    for (size_t i = 0; i < src.size(); ++i) {
        if (src[i] == '\\' && i + 3 < src.size() && src[i+1] == 'x') {
            // 尝试解析十六进制
            try {
                std::string hex = src.substr(i + 2, 2);
                char c = static_cast<char>(std::stoi(hex, nullptr, 16));
                dst += c;
                i += 3; // 跳过 \xNN
            } catch (...) {
                dst += src[i];
            }
        } else {
            dst += src[i];
        }
    }
    return dst;
}

// 获取当前已连接的 SSID 和 BSSID (通过 iw dev wlan0 link)
std::string GetConnectedSsid(const std::string& iface)
{
    std::string cmd = "iw dev " + iface + " link";
    FILE* fp = popen(cmd.c_str(), "r");
    if (!fp) return "";

    char line[512];
    std::string ssid = "";
    while (fgets(line, sizeof(line), fp)) {
        std::string s(line);
        s.erase(0, s.find_first_not_of(" \t\r\n"));
        s.erase(s.find_last_not_of(" \t\r\n") + 1);
        
        if (s.rfind("SSID:", 0) == 0) {
            ssid = s.substr(5);
            ssid.erase(0, ssid.find_first_not_of(" "));
            break;
        }
    }
    pclose(fp);
    return UnescapeSsid(ssid);
}

std::vector<Wlan::WifiInfo> Wlan::ScanWifi(const std::string& iface)
{
    std::vector<WifiInfo> list;
    
    // 1. 确保网卡启动
    std::string upCmd = "ip link set " + iface + " up";
    system(upCmd.c_str());

    // 2. 获取当前连接的 SSID
    std::string connectedSsid = GetConnectedSsid(iface);
    
    // 3. 执行扫描
    std::string cmd = "iw dev " + iface + " scan";
    FILE* fp = popen(cmd.c_str(), "r");
    if (!fp) {
        perror("popen failed");
        return list;
    }

    char line[512];
    WifiInfo current;
    bool inBss = false;
    bool hasSsid = false;
    std::set<std::string> seenSsids; // 用于去重

    auto pushCurrent = [&]() {
        if (inBss && hasSsid) {
            // 去重：如果这个 SSID 已经收集过了，跳过
            if (seenSsids.find(current.ssid) == seenSsids.end()) {
                seenSsids.insert(current.ssid);
                // 判断是否是当前连接的 WiFi
                if (!connectedSsid.empty() && current.ssid == connectedSsid) {
                    current.isConnected = true;
                }
                list.push_back(current);
            }
        }
    };

    while (fgets(line, sizeof(line), fp)) {
        std::string s(line);
        s.erase(0, s.find_first_not_of(" \t\r\n"));
        s.erase(s.find_last_not_of(" \t\r\n") + 1);
        if (s.empty()) continue;

        if (s.rfind("BSS ", 0) == 0) {
            pushCurrent();
            current = WifiInfo();
            hasSsid = false;
            inBss = true;
            continue;
        }

        if (!inBss) continue;

        if (s.rfind("SSID:", 0) == 0) {
            current.rawSsid = s.substr(5);
            current.rawSsid.erase(0, current.rawSsid.find_first_not_of(" "));
            
            // 过滤隐藏网络
            if (!current.rawSsid.empty() &&
                current.rawSsid != "\\x00\\x00\\x00\\x00\\x00\\x00\\x00\\x00") {
                
                current.ssid = UnescapeSsid(current.rawSsid);
                if (!current.ssid.empty()) {
                    hasSsid = true;
                }
            }
        }
        else if (s.rfind("signal:", 0) == 0) {
            std::string val = s.substr(7);
            try {
                current.signalDbm = static_cast<int>(std::stof(val));
            } catch (...) {
                current.signalDbm = 0;
            }
        }
        else if (s.rfind("RSN:", 0) == 0 || s.rfind("WPA:", 0) == 0) {
            current.isOpen = false;
        }
    }

    pushCurrent();
    pclose(fp);
    return list;
}

nlohmann::json Wlan::ScabWifiJson(const std::string& iface)
{
    nlohmann::json out;
    std::vector<Wlan::WifiInfo> list = Wlan::ScanWifi("wlan0");

    out["size"] = list.size();
    out["array"] = nlohmann::json::array();

    for (const Wlan::WifiInfo& w : list) {
        nlohmann::json dateNode = {
            {"ssid", w.ssid},
            {"signal", w.signalDbm},
            {"isopen", w.isOpen},
            {"isconnect", w.isConnected},
        };

        out["array"].push_back(std::move(dateNode));
    }

    return out;
}

bool Wlan::ConnectWifi(const std::string& ssid, const std::string& pswd,
                       std::string& error, const std::string& iface)
{
    error.clear();

    // 1. 确保网卡启动
    std::string upCmd = "ip link set " + iface + " up";
    system(upCmd.c_str());

    // 2. 生成新 network 块
    std::string tmpConf = "/tmp/wpa_new.conf";
    std::string genCmd;

    if (pswd.empty()) {
        // 开放网络：手动生成 key_mgmt=NONE 块
        std::ofstream ofs(tmpConf, std::ios::trunc);
        if (!ofs.is_open()) {
            error = "Failed to create temp config";
            return false;
        }
        ofs << "network={\n";
        ofs << "    ssid=\"" << ssid << "\"\n";
        ofs << "    key_mgmt=NONE\n";
        ofs << "}\n";
        ofs.close();
    } else {
        // 加密网络：用 wpa_passphrase 生成 WPA-PSK 块
        genCmd = "wpa_passphrase \"" + ssid + "\" \"" + pswd + "\" > " + tmpConf;
        if (system(genCmd.c_str()) != 0) {
            error = "Failed to generate wpa config";
            return false;
        }
        std::ifstream ifs(tmpConf);
        if (!ifs.is_open() || ifs.peek() == std::ifstream::traits_type::eof()) {
            error = "Generated wpa config is empty";
            return false;
        }
        ifs.close();
    }

    // 3. 读取新 network 块内容
    std::ifstream ifs(tmpConf);
    if (!ifs.is_open()) {
        error = "Failed to read wpa config";
        return false;
    }
    std::string netBlock((std::istreambuf_iterator<char>(ifs)),
                          std::istreambuf_iterator<char>());
    ifs.close();
    system(("rm -f " + tmpConf).c_str());

    // 4. 直接覆盖主配置，只保留新 network 块
    std::string confPath = "/etc/wpa_supplicant.conf";
    {
        std::ofstream ofs(confPath, std::ios::trunc);
        if (!ofs.is_open()) {
            error = "Failed to write main config";
            return false;
        }
        ofs << netBlock;
        ofs.close();
    }

    // 5. 清理残留进程和 socket
    system("killall wpa_supplicant 2>/dev/null");
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    system(("rm -f /var/run/wpa_supplicant/" + iface).c_str());

    // 6. 启动 wpa_supplicant
    std::string startCmd = "wpa_supplicant -B -i " + iface + " -c " + confPath;
    if (system(startCmd.c_str()) != 0) {
        error = "Failed to start wpa_supplicant";
        return false;
    }

    // 7. 用 iw 等待连接完成（最多 10 秒）
    for (int i = 0; i < 20; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(500));

        std::string checkCmd =
            "iw dev " + iface + " link 2>/dev/null | grep -q 'Connected to'";
        if (system(checkCmd.c_str()) == 0) {
            std::string dhcpCmd = "udhcpc -i " + iface + " -b >/dev/null 2>&1";
            system(dhcpCmd.c_str());
            return true;
        }
    }

    error = "Connect timeout";
    return false;
}
