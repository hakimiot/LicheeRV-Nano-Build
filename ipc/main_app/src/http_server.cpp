#include "mainapp_log.h"
#include "random.h"
#include "device_safe.h"
#include "http_server.h"

HttpServer::HttpServer(DataContext& context)
    : m_context(context)
{
    m_running = false;
    ServerInit();
}

HttpServer::~HttpServer()
{
    stop();
}

void HttpServer::start()
{
    if (m_running) return;
    m_running = true;
    m_serverThread = std::thread(&HttpServer::ServerLoop, this);
    m_timerThread = std::thread(&HttpServer::TimerLoop, this);
    LOG_I << "HTTP server start";
}

void HttpServer::stop()
{
    if (!m_running) return;

    m_running = false;

    m_svr.stop();

    LOG_I << "wait server thread";
    if (m_serverThread.joinable()) {
        m_serverThread.join();
    }

    LOG_I << "wait timer thread";
    if (m_timerThread.joinable()) {
        m_timerThread.join();
    }
    
    LOG_I << "HTTP server stop";
}

void HttpServer::ServerInit()
{
    m_svr.Post("/api/user/login", [this](const httplib::Request& req, httplib::Response& res) { // 用户登录
        nlohmann::json body = nlohmann::json::parse(req.body);
        nlohmann::json resp;

        std::string username = body["username"];
        std::string password = body["password"];
        std::string clientIp = req.remote_addr;

        bool ret = DeviceSafe::Authenticate(username, password);
        if (ret == true) {
            res.status = 200;
            resp = {
                {"token", GetToken(clientIp)},
                {"ip", clientIp}
            };
        } else {
            res.status = 500;
            resp = {
                {"error", "Incorrect username or password"}
            };
        }
        
        SetJson(res, resp);
    });

    m_svr.Post("/api/app/config", [this](const httplib::Request& req, httplib::Response& res) {    // 摄像头
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
                }}
            }}
        };

        SetJson(res, resp);
    });

    m_svr.set_error_handler([](const httplib::Request& req, httplib::Response& res) {
        std::string path = req.path;
    
        res.status = 404;
        res.set_content(R"({"error": "API endpoint not found", "path": ")" + path + R"("})",  "application/json");
    });
}

void HttpServer::ServerLoop()
{
    if (m_svr.listen("0.0.0.0", 9000) == false) {
        LOG_E << "HTTP server start failed";
    }
}

void HttpServer::TimerLoop()
{
    while (m_running) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        m_runningTime++;
        
        for (auto it = tokenInfos.begin(); it != tokenInfos.end();) { // 清理过期token
            if (m_runningTime - it->second.createTime > 86400 * 7) {  // 7天
                it = tokenInfos.erase(it);
                LOG_D << "clear timeout token, ip: " << it->first << ", token: " << it->second.token;
            } else {
                ++it;
            }
        }
    }
}

std::string HttpServer::GetToken(std::string ip)
{
    std::string token = Random::GetRandomString(32);
    TokenInfo_t tokenInfo = {token, m_runningTime};
    tokenInfos[ip] = tokenInfo;
    LOG_D << "for ip: " << ip << "create login token: " << token;
    return token;
}

int HttpServer::VerifyToken(std::string ip, std::string token)
{
    auto it = tokenInfos.find(ip);
    if (it != tokenInfos.end() && it->second.token == token) {
        LOG_D << "token verify success, ip: " << ip << ", token: " << token;
        return 0;
    }

    LOG_D << "token verify failed, ip: " << ip << ", token: " << token;
    return -1;
}

void HttpServer::SetJson(httplib::Response& res, const nlohmann::json& j, int code)
{
    res.status = code;
    res.set_header("Content-Type", "application/json; charset=utf-8");
    res.set_content(j.dump(), "application/json");
}
