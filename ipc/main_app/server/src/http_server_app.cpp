#include "mainapp_log.h"
#include "http_server.h"

void HttpServer::ServerInitApp()
{
    m_svr.Post("/api/app/config", [this](const httplib::Request& req, httplib::Response& res) {
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
            }},
            {"rtmp", {
                {"live", {
                    {"url", m_context.liveStreamer.GetRtmpUrl()},
                    {"connect", m_context.liveStreamer.IsConnected()},
                    {"stream", m_context.liveStreamer.IsStreaming()}
                }},
                {"replay", {
                    {"url", m_context.replayStreamer.GetRtmpUrl()},
                    {"connect", m_context.replayStreamer.IsConnected()},
                    {"stream", m_context.replayStreamer.IsStreaming()}
                }}
            }},
            {"info", {
                {"device", "LicheeRV-Nano"},
                {"version", "1.0.0"},
                {"uptime", 3600}
            }}
        };

        SetJson(res, resp);
    });

    m_svr.Post("/api/settings/info", [this](const httplib::Request& req, httplib::Response& res) {
        nlohmann::json body = nlohmann::json::parse(req.body);
        nlohmann::json resp;

        std::string token = body["token"];
        std::string clientIp = req.remote_addr;

        VRRIFY_CODE(clientIp, token, resp, res);

        resp = {
            {"code", 200},
            {"name", "device-01"},
            {"interval", 1000}
        };

        SetJson(res, resp);
    });
}
