#include "mainapp_log.h"
#include "network.h"
#include "http_server.h"

void HttpServer::ServerInitApp()
{
    /**
     * @brief 获取应用配置
     */
    m_svr.Post("/api/app/config", [this](const httplib::Request& req, httplib::Response& res) {
        nlohmann::json body = nlohmann::json::parse(req.body);
        nlohmann::json resp;

        std::string token = body["token"];
        std::string clientIp = req.remote_addr;

        VRRIFY_CODE(clientIp, token, resp, res);

        DeviceInfo::CpuInfo cpuInfo;
        DeviceInfo::MemoryInfo memoryInfo;
        DeviceInfo::DiskInfo diskInfo;
        std::string error;

        DeviceInfo::GetCpuInfo(cpuInfo, error);
        DeviceInfo::GetMemoryInfo(memoryInfo, error);
        DeviceInfo::GetDiskInfo("/", diskInfo, error);

        std::string interface = NetworkInfo::GetDefaultInterface();
        NetworkInfo::InterfaceInfo interfaceInfo = NetworkInfo::GetInterfaceInfo(interface);
        std::string ipv4 = "0.0.0.0";
        std::string ipv6 = "0.0.0.0";
        if (!interfaceInfo.ipv4.empty()) {
            ipv4 = interfaceInfo.ipv4[0].address;
        }
        if (!interfaceInfo.ipv6.empty()) {
            ipv6 = interfaceInfo.ipv6[0].address;
        }

        resp = {
            {"code", 200},
            {"data", {
                {"network", {
                    {"interface", interface},
                    {"ip", {
                        {"ipv4", ipv4},
                        {"ipv6", ipv6}
                    }}
                }},
                {"capture", {
                    {"width", m_context.cameraCapture.config().width},
                    {"height", m_context.cameraCapture.config().height},
                    {"fps", m_context.cameraCapture.config().srcFps}
                }},
                {"rtmp", {
                    {"live", {
                        {"url", m_context.liveStreamer.GetRtmpUrl()},
                        {"connect", m_context.liveStreamer.IsConnected()},
                        {"stream", m_context.liveStreamer.IsStreaming()}
                    }},
                    {"replay", {
                        {"url", m_context.replayStreamer.GetRtmpUrl()},
                        {"connect", m_context.replayStreamer.IsConnected()},
                        {"stream", m_context.replayStreamer.IsStreaming()}
                    }}
                }},
                {"system", {
                    {"info", {
                        {"key", m_context.deviceInfo.key},
                        {"name", m_context.deviceInfo.name},
                        {"version", m_context.deviceInfo.version}
                    }},
                    {"cpu", {
                        {"load", cpuInfo.load},
                        {"bootTime", cpuInfo.bootTime},
                        {"temperature", cpuInfo.temperature}
                    }},
                    {"memory", {
                        {"total", memoryInfo.total},
                        {"used", memoryInfo.used},
                        {"free", memoryInfo.free}
                    }},
                    {"disk", {
                        {"total", diskInfo.total},
                        {"used", diskInfo.used},
                        {"free", diskInfo.free}
                    }}
                }}
            }}
        };

        SetJson(res, resp);
    });

    m_svr.Post("/api/settings/info", [this](const httplib::Request& req, httplib::Response& res) {
        nlohmann::json body = nlohmann::json::parse(req.body);
        nlohmann::json resp;

        std::string token = body["token"];
        std::string clientIp = req.remote_addr;

        VRRIFY_CODE(clientIp, token, resp, res);

        resp = {
            {"code", 200},
            {"data", {
                {"name", m_context.deviceInfo.key},
                {"interval", 1000}
            }}
        };

        SetJson(res, resp);
    });
}
