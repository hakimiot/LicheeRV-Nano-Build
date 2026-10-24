#include <SimpleIni.h>
#include "mainapp_log.h"
#include "http_server.h"

void HttpServer::ServerInitCapture()
{
    m_svr.Post("/api/capture/get", [this](const httplib::Request& req, httplib::Response& res) {
        nlohmann::json body = nlohmann::json::parse(req.body);
        nlohmann::json resp;

        std::string token = body["token"];
        std::string clientIp = req.remote_addr;

        VRRIFY_CODE(clientIp, token, resp, res);

        resp = {
            {"code", 200},
            {"capture", {
                {"width", m_context.cameraCapture.config().width},
                {"height", m_context.cameraCapture.config().height},
                {"fps", m_context.cameraCapture.config().srcFps}
            }}
        };

        SetJson(res, resp);
    });
    
    m_svr.Post("/api/capture/set", [this](const httplib::Request& req, httplib::Response& res) {
        nlohmann::json body = nlohmann::json::parse(req.body);
        nlohmann::json resp;

        std::string token = body["token"];
        std::string clientIp = req.remote_addr;

        VRRIFY_CODE(clientIp, token, resp, res);

        CSimpleIniA ini;
        SI_Error rc = ini.LoadFile("/etc/main_app/config.ini");
        if (rc < 0) {
            resp = {
                {"code", 500},
                {"error", "failed to load config"}
            };
            SetJson(res, resp);
            return;
        }

        CameraConfig cfg = m_context.cameraCapture.config();
        
        if (body.contains("width"))  cfg.width  = body["width"];
        if (body.contains("height")) cfg.height = body["height"];
        if (body.contains("fps"))    cfg.srcFps = body["fps"];

        ini.SetLongValue("capture", "width", cfg.width);
        ini.SetLongValue("capture", "height", cfg.height);
        ini.SetLongValue("capture", "fps", cfg.srcFps);

        rc = ini.SaveFile("/etc/main_app/config.ini");
        if (rc < 0) {
            resp = {
                {"code", 500},
                {"error", "failed to save config"}
            };
        } else {
            resp = {
                {"code", 200},
                {"msg", "success"}
            };
        }

        SetJson(res, resp);
    });
}
