#include "wlan.h"
#include "http_server.h"

void HttpServer::ServerInitNetwork()
{
    m_svr.Post("/api/network/wifilist", [this](const httplib::Request& req, httplib::Response& res) {
        nlohmann::json body = nlohmann::json::parse(req.body);
        nlohmann::json resp;

        std::string token = body["token"];
        std::string clientIp = req.remote_addr;

        VRRIFY_CODE(clientIp, token, resp, res);

        resp = Wlan::ScabWifiJson("wlan0");
        resp["code"] = 200;

        SetJson(res, resp);
    });

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
            resp = {{"code", 200}, {"msg", "success"}};
        } else { 
            resp = {{"code", 500}, {"error", error}};
        }

        SetJson(res, resp);
    });

    m_svr.Post("/api/network/savewifi", [this](const httplib::Request& req, httplib::Response& res) {
        nlohmann::json body = nlohmann::json::parse(req.body);
        nlohmann::json resp;

        std::string token = body["token"];
        std::string clientIp = req.remote_addr;

        VRRIFY_CODE(clientIp, token, resp, res);

        resp = Wlan::ScabWifiJson("wlan0");
        resp["code"] = 200;

        SetJson(res, resp);
    });
}
