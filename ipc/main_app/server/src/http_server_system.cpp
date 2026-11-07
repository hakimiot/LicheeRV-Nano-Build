#include "power_manage.h"
#include "http_server.h"

void HttpServer::ServerInitSystem()
{
    /**
     * @brief 重启置系统
     * @param token 认证令牌
     */
    m_svr.Post("/api/system/reset", [this](const httplib::Request& req, httplib::Response& res) {
        nlohmann::json body = nlohmann::json::parse(req.body);
        nlohmann::json resp;

        std::string token = body["token"];
        std::string clientIp = req.remote_addr;

        VRRIFY_CODE(clientIp, token, resp, res);

        resp = {{"code", 200}, {"data", {}}};
        SetJson(res, resp);

        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        Power::reset();
    });

    /**
     * @brief 获取设备信息
     */
    m_svr.Post("/api/system/info", [this](const httplib::Request& req, httplib::Response& res) {
        nlohmann::json body = nlohmann::json::parse(req.body);
        nlohmann::json resp;

        std::string token = body["token"];
        std::string clientIp = req.remote_addr;

        VRRIFY_CODE(clientIp, token, resp, res);

        resp = {
            {"code", 200},
            {"data", {
                {"key", m_context.deviceInfo.key},
                {"name", m_context.deviceInfo.name},
                {"version", m_context.deviceInfo.version}
            }}
        };
        SetJson(res, resp);
    });

    /**
     * @brief 获取内存使用情况
     */
    m_svr.Post("/api/system/memory", [this](const httplib::Request& req, httplib::Response& res) {
        nlohmann::json body = nlohmann::json::parse(req.body);
        nlohmann::json resp;

        std::string token = body["token"];
        std::string clientIp = req.remote_addr;

        VRRIFY_CODE(clientIp, token, resp, res);

        DeviceInfo::MemoryInfo memoryInfo;
        std::string error;

        if (DeviceInfo::GetMemoryInfo(memoryInfo, error)) {
            resp = {
                {"code", 200},
                {"data", {
                    {"total", memoryInfo.total},
                    {"used", memoryInfo.used},
                    {"free", memoryInfo.free}
                }}
            };
        } else {
            resp = {
                {"code", 500},
                {"error", error}
            };
        }
        SetJson(res, resp);
    });

    /**
     * @brief 获取磁盘信息
     */
    m_svr.Post("/api/system/disk", [this](const httplib::Request& req, httplib::Response& res){
        nlohmann::json body = nlohmann::json::parse(req.body);
        nlohmann::json resp;

        std::string token = body["token"];
        std::string clientIp = req.remote_addr;

        VRRIFY_CODE(clientIp, token, resp, res);

        DeviceInfo::DiskInfo diskInfo;
        std::string error;
        if (DeviceInfo::GetDiskInfo("/", diskInfo, error)) {
            resp = {
                {"code", 200},
                {"data", {
                    {"total", diskInfo.total},
                    {"used", diskInfo.used},
                    {"free", diskInfo.free}
                }}
            };
        } else {
            resp = {
                {"code", 500},
                {"error", error}
            };
        }
        SetJson(res, resp);
    });

    /**
     * @brief 获取CPU信息
     */
    m_svr.Post("/api/system/cpu", [this](const httplib::Request& req, httplib::Response& res) {
        nlohmann::json body = nlohmann::json::parse(req.body);
        nlohmann::json resp;

        std::string token = body["token"];
        std::string clientIp = req.remote_addr;

        VRRIFY_CODE(clientIp, token, resp, res);

        DeviceInfo::CpuInfo cpuInfo;
        std::string error;
        if (DeviceInfo::GetCpuInfo(cpuInfo, error)) {
            resp = {
                {"code", 200},
                {"data", {
                    {"load", cpuInfo.load},
                    {"bootTime", cpuInfo.bootTime},
                    {"temperature", cpuInfo.temperature}
                }}
            };
        } else {
            resp = {
                {"code", 500},
                {"error", error}
            };
        }
        SetJson(res, resp);
    });
}
