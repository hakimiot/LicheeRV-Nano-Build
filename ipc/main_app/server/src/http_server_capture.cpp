#include <SimpleIni.h>
#include "mainapp_log.h"
#include "http_server.h"

void HttpServer::ServerInitCapture()
{
    /**
     * 获取摄像头采集参数
     */
    m_svr.Post("/api/capture/get", [this](const httplib::Request& req, httplib::Response& res) {
        nlohmann::json body = nlohmann::json::parse(req.body);
        nlohmann::json resp;

        std::string token = body["token"];
        std::string clientIp = req.remote_addr;

        VRRIFY_CODE(clientIp, token, resp, res);

        res.status = 200;
        resp = {
            {"capture", {
                {"width", m_context.cameraCapture.config().width},      // 图像宽
                {"height", m_context.cameraCapture.config().height},    // 图像高
                {"fps", m_context.cameraCapture.config().srcFps}        // 帧率
            }}
        };

        SetJson(res, resp);
    });
    
    /**
     * 配置摄像头采集参数
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
            res.status = 500;
            LOG_E << "Failed to load config";
            resp = {
                {"error", "failed to load config, error" + rc}
            };
            SetJson(res, resp);
        }

        CameraConfig cfg = m_context.cameraCapture.config();
        
        if (body.contains("width")) {
            cfg.width = body["width"];
        }

        if (body.contains("height")) {
            cfg.height = body["height"];
        }

        if (body.contains("fps")) {
            cfg.srcFps = body["fps"];
        }

        res.status = 200;
        rc = ini.LoadFile("/etc/main_app/config.ini");
        if (rc < 0) {
            res.status = 500;
            LOG_E << "failed to load config";
            resp = {
                {"error", "failed to load config"}
            };
        } else {
            ini.SetLongValue("capture", "width", cfg.width);
            ini.SetLongValue("capture", "height", cfg.height);
            ini.SetLongValue("capture", "fps", cfg.srcFps);

            rc = ini.SaveFile("/etc/main_app/config.ini");
            if (rc < 0) {
                res.status = 500;
                LOG_E << "Failed to save config, error=" << rc;
                resp = {
                    {"error", "failed to save config, error" + rc}
                };
            }
        }

        SetJson(res, resp);
    });
}
