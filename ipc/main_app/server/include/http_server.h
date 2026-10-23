#pragma once

#include <httplib.h>
#include <nlohmann/json.hpp>
#include "camera_capture.h"
#include "rtmp_streamer.h"
#include "video_reader.h"

#define VRRIFY_CODE(ip, token, resp, res) do { \
    int ret = VerifyToken(ip, token); \
    if (ret < 0) { \
        res.status = 500; \
        resp = { \
            {"error", "token verift failed"} \
        }; \
        SetJson(res, resp); \
        return; \
    } \
} while (0)

struct DataContext {
    std::atomic<bool>& running;
    CameraCapture& cameraCapture;
    RTMPStreamer& liveStreamer;
    RTMPStreamer& replayStreamer;
    VideoReader& videoReader;
};

struct TokenInfo_t {
    std::string token;
    uint64_t createTime;
};

class HttpServer
{
public:
    HttpServer(DataContext& context);
    ~HttpServer();
    void start();
    void stop();

private:
    void ServerInit();
    void ServerLoop();
    void TimerLoop();

    void ServerInitApp();       // APP相关 API
    void ServerInitCapture();   // 采集相关 API
    void ServerInitMedia();     // 媒体相关 API
    void ServerInitNetwork();   // 网络相关 API
    void ServerInitStream();    // 推流相关 API
    void ServerInitSystem();    // 系统相关 API
    void ServerInitUser();      // 用户相关 API
    
    std::string GetToken(std::string ip);
    int VerifyToken(std::string ip, std::string token);

    void SetJson(httplib::Response& res, const nlohmann::json& j, int code = 200);

    httplib::Server m_svr;
    std::thread m_serverThread;
    std::thread m_timerThread;
    std::atomic<bool> m_running;
    DataContext& m_context;

    uint64_t m_runningTime = 0;
    
    std::map<std::string, TokenInfo_t> tokenInfos;
};
