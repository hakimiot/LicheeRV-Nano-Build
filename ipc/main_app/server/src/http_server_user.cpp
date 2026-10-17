#include "device_safe.h"
#include "http_server.h"

void HttpServer::ServerInitUser()
{
    /**
     * 用户登录
     */
    m_svr.Post("/api/user/login", [this](const httplib::Request& req, httplib::Response& res) {
        nlohmann::json body = nlohmann::json::parse(req.body);
        nlohmann::json resp;

        std::string username = body["username"];
        std::string password = body["password"];
        std::string clientIp = req.remote_addr;

        bool ret = DeviceSafe::Authenticate(username, password);
        if (ret == true) {
            res.status = 200;
            resp = {
                {"token", GetToken(clientIp)},
                {"ip", clientIp}
            };
        } else {
            res.status = 500;
            resp = {
                {"error", "Incorrect username or password"}
            };
        }
        
        SetJson(res, resp);
    });
}
