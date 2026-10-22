#pragma once

#include <string>
#include <vector>
#include <nlohmann/json.hpp>

namespace Wlan
{
    struct WifiInfo {
        std::string ssid;
        std::string rawSsid;
        int signalDbm = 0;
        bool isOpen = true;
        bool isConnected = false;
    };

    std::vector<WifiInfo> ScanWifi(const std::string& iface = "wlan0");
    nlohmann::json ScabWifiJson(const std::string& iface = "wlan0");
    bool ConnectWifi(const std::string& ssid, const std::string& pswd, std::string& error, const std::string& iface = "wlan0");
};
