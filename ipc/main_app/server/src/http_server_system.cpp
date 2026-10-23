#include "power_manage.h"
#include "http_server.h"

void HttpServer::ServerInitSystem()
{
    /**
     * 重启设备
     */
    m_svr.Post("/api/system/reset", [this](const httplib::Request& req, httplib::Response& res) {
        nlohmann::json body = nlohmann::json::parse(req.body);
        nlohmann::json resp;

        std::string token = body["token"];
        std::string clientIp = req.remote_addr;

        VRRIFY_CODE(clientIp, token, resp, res);

        Power::reset();
    });
}
