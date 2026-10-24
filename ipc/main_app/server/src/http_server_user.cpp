#include "device_safe.h"
#include "http_server.h"

void HttpServer::ServerInitUser()
{
    m_svr.Post("/api/user/login", [this](const httplib::Request& req, httplib::Response& res) {
        nlohmann::json body = nlohmann::json::parse(req.body);
        nlohmann::json resp;

        std::string username = body["username"];
        std::string password = body["password"];
        std::string clientIp = req.remote_addr;

        bool ret = DeviceSafe::Authenticate(username, password);
        if (ret) {
            resp = {
                {"code", 200},
                {"token", GetToken(clientIp)},
                {"ip", clientIp}
            };
        } else {
            resp = {
                {"code", 401},
                {"error", "Incorrect username or password"}
            };
        }
        
        SetJson(res, resp);
    });
}
