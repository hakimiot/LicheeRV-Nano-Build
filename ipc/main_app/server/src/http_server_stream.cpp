#include "mainapp_log.h"
#include "http_server.h"

void HttpServer::ServerInitStream()
{
    /**
     * 获取/设置 直播推流状态
     */
    m_svr.Post("/api/stream/live", [this](const httplib::Request& req, httplib::Response& res) {
        nlohmann::json body = nlohmann::json::parse(req.body);
        nlohmann::json resp;

        std::string token = body["token"];
        std::string mode = body["mode"];
        std::string clientIp = req.remote_addr;

        VRRIFY_CODE(clientIp, token, resp, res);

        if (mode == "set") {
            res.status = 200;
            bool status = body["status"];
            if (status == true) {
                m_context.liveStreamer.Start();
            } else {
                m_context.liveStreamer.Stop();
            }
        } else if (mode == "get") {
            res.status = 200;
            resp = {
                {"connect", m_context.liveStreamer.IsConnected()},  // 服务器连接状态
                {"stream", m_context.liveStreamer.IsStreaming()}    // 直播推流状态
            };
        } else {
            res.status = 500;
            resp = {
                {"error", "mode: \"" + mode + "\" is illegal"}
            };
        }

        SetJson(res, resp);
    });
    
    /**
     * 获取/设置 回放推流状态
     */
    m_svr.Post("/api/stream/replay", [this](const httplib::Request& req, httplib::Response& res) {
        nlohmann::json body = nlohmann::json::parse(req.body);
        nlohmann::json resp;

        std::string token = body["token"];
        std::string mode = body["mode"];
        std::string clientIp = req.remote_addr;

        VRRIFY_CODE(clientIp, token, resp, res);

        if (mode == "set") {
            res.status = 200;
            std::string status = body["status"];
            if (status == "media") {    // 设置播放文件
                std::string file = body["file"];
                if (!m_context.videoReader.Open(file)) {
                    LOG_E << "VideoReader open failed: " << file;
                    res.status = 500;
                    resp = {
                        {"error", "open: \"" + file + "\" failed"}
                    };
                } else {
                    resp = {
                        {"url", m_context.replayStreamer.GetRtmpUrl()}
                    };
                }
            } else if (status == "start") {    // 开始播放
                m_context.replayStreamer.Start();
                m_context.videoReader.Play();
            } else if (status == "paused") {  // 暂停播放
                m_context.videoReader.Pause();
            } else if (status == "stop") {  // 停止播放
                m_context.videoReader.Stop();
                m_context.videoReader.Close();
                m_context.replayStreamer.Stop();
            } else if (status == "seek") {  // 设置进度
                int64_t targetTimeMs = body["seek"];
                if (!m_context.videoReader.Seek(targetTimeMs)) {
                    res.status = 500;
                    resp = {
                        {"error", "seel failed"}
                    };
                }
            } else {  // 错误的指令
                res.status = 500;
                resp = {
                    {"error", "status: \"" + status + "\" is illegal"}
                };
            }
        } else if (mode == "get") {
            res.status = 200;
            resp = {
                {"connect", m_context.replayStreamer.IsConnected()},  // 服务器连接状态
                {"stream", m_context.replayStreamer.IsStreaming()},   // 直播推流状态
                {"reader", {
                    {"state", m_context.videoReader.GetStateString()}   // 视频读取器状态
                }}
            };
        } else {
            res.status = 500;
            resp = {
                {"error", "mode: \"" + mode + "\" is illegal"}
            };
        }

        SetJson(res, resp);
    });
}
