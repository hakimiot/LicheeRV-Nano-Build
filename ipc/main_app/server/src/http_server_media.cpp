#include "file_system.h"
#include "http_server.h"

void HttpServer::ServerInitMedia()
{
    m_svr.Post("/api/media/list", [this](const httplib::Request& req, httplib::Response& res) {
        nlohmann::json body = nlohmann::json::parse(req.body);
        nlohmann::json resp;
        std::string date = "";
        std::string error;

        std::string token = body["token"];
        std::string clientIp = req.remote_addr;
        
        VRRIFY_CODE(clientIp, token, resp, res);

        if (body.contains("date")) {
            date = body.value("date", "");
        }

        nlohmann::json out;
        if (FileSystem::ScanMediaJson(out, error, date) != true) {
            resp = {{"code", 500}, {"error", error}};
        } else {
            resp = out;
            resp["code"] = 200;
        }

        SetJson(res, resp);
    });
}
