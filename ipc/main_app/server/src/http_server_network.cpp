#include "wlan.h"
#include "http_server.h"

void HttpServer::ServerInitNetwork()
{
    /**
     * 获取wifi列表
     */
    m_svr.Post("/api/network/wifilist", [this](const httplib::Request& req, httplib::Response& res) {    // 
        nlohmann::json body = nlohmann::json::parse(req.body);
        nlohmann::json resp;

        std::string token = body["token"];
        std::string clientIp = req.remote_addr;

        VRRIFY_CODE(clientIp, token, resp, res);

        res.status = 200;
        resp = Wlan::ScabWifiJson("wlan0");

        SetJson(res, resp);
    });

    /**
     * 连接wifi
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
        if (Wlan::ConnectWifi(ssid, pswd, error) == true) {   
            res.status = 200;
        } else { 
            res.status = 500;
            resp = {
                {"error", error}
            };
        }

        SetJson(res, resp);
    });

    /**
     * 获取已保存的wifi列表
     */
    m_svr.Post("/api/network/savewifi", [this](const httplib::Request& req, httplib::Response& res) {
        nlohmann::json body = nlohmann::json::parse(req.body);
        nlohmann::json resp;

        std::string token = body["token"];
        std::string clientIp = req.remote_addr;

        VRRIFY_CODE(clientIp, token, resp, res);

        res.status = 200;
        resp = Wlan::ScabWifiJson("wlan0");

        SetJson(res, resp);
    });
}
