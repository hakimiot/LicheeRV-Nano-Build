#include "wlan.h"
#include "network.h"
#include "http_server.h"

void HttpServer::ServerInitNetwork()
{
    /**
     * @brief 获取WiFi列表
     */
    m_svr.Post("/api/network/wifilist", [this](const httplib::Request& req, httplib::Response& res) {
        nlohmann::json body = nlohmann::json::parse(req.body);
        nlohmann::json resp;

        std::string token = body["token"];
        std::string clientIp = req.remote_addr;

        VRRIFY_CODE(clientIp, token, resp, res);

        nlohmann::json wifiList = Wlan::ScabWifiJson("wlan0");
        resp = {
            {"code", 200},
            {"data", wifiList}
        };

        SetJson(res, resp);
    });

    /**
     * @brief 连接WiFi
     * @param ssid SSID
     * @param pswd 密码
     */
    m_svr.Post("/api/network/connect", [this](const httplib::Request& req, httplib::Response& res) {
        nlohmann::json body = nlohmann::json::parse(req.body);
        nlohmann::json resp;

        std::string token = body["token"];
        std::string clientIp = req.remote_addr;

        VRRIFY_CODE(clientIp, token, resp, res);

        std::string ssid = body["ssid"];
        std::string pswd = body["pswd"];

        std::string error;
        if (Wlan::ConnectWifi(ssid, pswd, error)) {
            resp = {{"code", 200}, {"data", {}}};
        } else { 
            resp = {{"code", 500}, {"error", error}};
        }

        SetJson(res, resp);
    });

    /**
     * @brief 获取默认网卡信息
     */
    m_svr.Post("/api/network/default", [this](const httplib::Request& req, httplib::Response& res) {
        nlohmann::json body = nlohmann::json::parse(req.body);
        nlohmann::json resp;

        std::string token = body["token"];
        std::string clientIp = req.remote_addr;

        VRRIFY_CODE(clientIp, token, resp, res);

        std::string defaultInterface = NetworkInfo::GetDefaultInterface();
        resp = {
            {"code", 200},
            {"data", {
                {"info", NetworkInfo::GetInterfaceInfoJson(defaultInterface)}
            }}
        };

        SetJson(res, resp);
    });

    /**
     * @brief 获取所有网卡信息
     */
    m_svr.Post("/api/network/all", [this](const httplib::Request& req, httplib::Response& res) {
        nlohmann::json body = nlohmann::json::parse(req.body);
        nlohmann::json resp;

        std::string token = body["token"];
        std::string clientIp = req.remote_addr;

        VRRIFY_CODE(clientIp, token, resp, res);

        resp = {
            {"code", 200},
            {"data", {
                {"list", NetworkInfo::GetAllInterfacesJson()}
            }}
        };

        SetJson(res, resp);
    });
}
