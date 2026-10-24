#include <SimpleIni.h>
#include "mainapp_log.h"
#include "http_server.h"

void HttpServer::ServerInitStream()
{
    m_svr.Post("/api/stream/live", [this](const httplib::Request& req, httplib::Response& res) {
        nlohmann::json body = nlohmann::json::parse(req.body);
        nlohmann::json resp;

        std::string token = body["token"];
        std::string mode = body["mode"];
        std::string clientIp = req.remote_addr;

        VRRIFY_CODE(clientIp, token, resp, res);

        if (mode == "set") {
            bool status = body["status"];
            if (status) m_context.liveStreamer.Start();
            else m_context.liveStreamer.Stop();
            resp = {{"code", 200}, {"msg", "success"}};
        } else if (mode == "get") {
            resp = {
                {"code", 200},
                {"connect", m_context.liveStreamer.IsConnected()},
                {"stream", m_context.liveStreamer.IsStreaming()}
            };
        } else {
            resp = {
                {"code", 500},
                {"error", "mode: \"" + mode + "\" is illegal"}
            };
        }

        SetJson(res, resp);
    });
    
    m_svr.Post("/api/stream/replay", [this](const httplib::Request& req, httplib::Response& res) {
        nlohmann::json body = nlohmann::json::parse(req.body);
        nlohmann::json resp;

        std::string token = body["token"];
        std::string mode = body["mode"];
        std::string clientIp = req.remote_addr;

        VRRIFY_CODE(clientIp, token, resp, res);

        if (mode == "set") {
            std::string status = body["status"];
            if (status == "media") {
                std::string file = body["file"];
                if (!m_context.videoReader.Open(file)) {
                    resp = {{"code", 500}, {"error", "open: \"" + file + "\" failed"}};
                } else {
                    resp = {{"code", 200}, {"url", m_context.replayStreamer.GetRtmpUrl()}};
                }
            } else if (status == "start") {
                m_context.replayStreamer.Start();
                m_context.videoReader.Play();
                resp = {{"code", 200}, {"msg", "success"}};
            } else if (status == "paused") {
                m_context.videoReader.Pause();
                resp = {{"code", 200}, {"msg", "success"}};
            } else if (status == "stop") {
                m_context.videoReader.Stop();
                m_context.videoReader.Close();
                m_context.replayStreamer.Stop();
                resp = {{"code", 200}, {"msg", "success"}};
            } else if (status == "seek") {
                if (!body.contains("seek") || !body["seek"].is_number()) {
                    resp = {{"code", 400}, {"error", "missing seek value"}};
                } else {
                    int64_t targetTimeMs = body["seek"].get<int64_t>();
                    if (!m_context.videoReader.Seek(targetTimeMs)) {
                        resp = {{"code", 500}, {"error", "seek failed or timeout"}};
                    } else {
                        resp = {{"code", 200}, {"seek", targetTimeMs}};
                    }
                }
            } else {
                resp = {{"code", 500}, {"error", "status: \"" + status + "\" is illegal"}};
            }
        } else if (mode == "get") {
            resp = {
                {"code", 200},
                {"connect", m_context.replayStreamer.IsConnected()},
                {"stream", m_context.replayStreamer.IsStreaming()},
                {"reader", {{"state", m_context.videoReader.GetStateString()}}}
            };
        } else {
            resp = {{"code", 500}, {"error", "mode: \"" + mode + "\" is illegal"}};
        }

        SetJson(res, resp);
    });

    m_svr.Post("/api/stream/seturl", [this](const httplib::Request& req, httplib::Response& res) {
        nlohmann::json body = nlohmann::json::parse(req.body);
        nlohmann::json resp;

        std::string token = body["token"];
        std::string clientIp = req.remote_addr;

        VRRIFY_CODE(clientIp, token, resp, res);

        CSimpleIniA ini;
        SI_Error rc = ini.LoadFile("/etc/main_app/config.ini");
        if (rc < 0) {
            resp = {{"code", 500}, {"error", "failed to load config"}};
            SetJson(res, resp);
            return;
        }

        std::string url = body["url"];
        ini.SetValue("rtmp", "url", url.c_str());

        rc = ini.SaveFile("/etc/main_app/config.ini");
        if (rc < 0) {
            resp = {{"code", 500}, {"error", "failed to save config"}};
        } else {
            resp = {{"code", 200}, {"msg", "success"}};
        }

        SetJson(res, resp);
    });
}
