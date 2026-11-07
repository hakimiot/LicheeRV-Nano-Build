#include <SimpleIni.h>
#include "log.h"
#include "http_server.h"

void HttpServer::ServerInitCapture()
{
    /**
     * @brief 获取摄像头采集配置
     */
    m_svr.Post("/api/capture/get", [this](const httplib::Request& req, httplib::Response& res) {
        nlohmann::json body = nlohmann::json::parse(req.body);
        nlohmann::json resp;

        std::string token = body["token"];
        std::string clientIp = req.remote_addr;

        VRRIFY_CODE(clientIp, token, resp, res);

        resp = {
            {"code", 200},
            {"data", {
                {"width", m_context.cameraCapture.config().width},
                {"height", m_context.cameraCapture.config().height},
                {"fps", m_context.cameraCapture.config().srcFps}
            }}
        };

        SetJson(res, resp);
    });

    /**
     * @brief 设置摄像头采集配置
     * @param width 宽度
     * @param height 高度
     * @param fps 帧率
     */
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
                {"data", {}}
            };
        }

        SetJson(res, resp);
    });
}
