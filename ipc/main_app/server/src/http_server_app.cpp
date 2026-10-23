#include "http_server.h"

void HttpServer::ServerInitApp()
{
    /**
     * 获取APP配置
     */
    m_svr.Post("/api/app/config", [this](const httplib::Request& req, httplib::Response& res) {
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
            }},
            {"rtmp", {
                {"live", {
                    {"url", m_context.liveStreamer.GetRtmpUrl()},       // 直播推流地址
                    {"connect", m_context.liveStreamer.IsConnected()},  // 服务器连接状态
                    {"stream", m_context.liveStreamer.IsStreaming()}    // 直播推流状态
                }},
                {"replay", {
                    {"url", m_context.replayStreamer.GetRtmpUrl()},       // 回放推流地址
                    {"connect", m_context.replayStreamer.IsConnected()},  // 服务器连接状态
                    {"stream", m_context.replayStreamer.IsStreaming()}    // 回放推流状态
                }}
            }}
        };

        SetJson(res, resp);
    });
}
