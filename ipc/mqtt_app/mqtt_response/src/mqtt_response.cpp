#include "log.h"
#include "mqtt_response.h"

MqttResponse::MqttResponse(HttpClient &httpClient)
    : m_httpClient(httpClient)
{
    RegisterMessageResponseHttp();

    m_typeMap["TypeList"] = {false, [this](const nlohmann::json &, nlohmann::json &res) {
        nlohmann::json array = nlohmann::json::array();
        for (auto &it : m_typeMap) {
            nlohmann::json item = {
                {"type", it.first},
                {"description", it.second.description},
                {"usage", it.second.parameters}
            };
            array.push_back(item);
        }
        res = {
            {"code", 200},
            {"type", "TypeList"},
            {"data", {
                {"list", array},
                {"size", array.size()}
            }}
        };
        return true;
    }, "", "获取所有支持的类型列表", "{}"};
}

MqttResponse::~MqttResponse()
{
}

namespace {

nlohmann::json ErrorResponse(int code, const std::string &type, const std::string &error)
{
    return {
        {"code", code},
        {"type", type},
        {"error", error}
    };
}

std::string DumpJson(const nlohmann::json &value)
{
    return value.dump(-1, ' ', false, nlohmann::json::error_handler_t::replace);
}

}

bool MqttResponse::MessageResponse(const std::string &req, std::string &res)
{
    try {
        nlohmann::json reqData = nlohmann::json::parse(req);
        nlohmann::json resData;

        std::string type;
        if (reqData.is_object() && reqData.contains("type") && reqData["type"].is_string()) {
            type = reqData["type"].get<std::string>();
        }
        if (type.empty()) {
            LOG_E << "Message response type is invalid";
            res = DumpJson(ErrorResponse(400, "invalid", "Message response type is invalid"));
            return true;
        }

        nlohmann::json data = nlohmann::json::object();
        if (reqData.contains("data") && reqData["data"].is_object()) {
            data = reqData["data"];
        }

        auto it = m_typeMap.find(type);
        if (it == m_typeMap.end()) {
            LOG_E << "Message response not found: " << type;
            res = DumpJson(ErrorResponse(404, type, "Message response not found: " + type));
            return true;
        }

        if (it->second.isHttp) {
            data["token"] = "";
            const std::string &httpUrl = it->second.httpUrl;
            httplib::Result result = m_httpClient.Post(httpUrl, DumpJson(data));
            if (result && result->status == 200) {
                const std::string body = result->body;
                resData = nlohmann::json::parse(body);
                if (!resData.is_object()) {
                    LOG_E << "HTTP response is not an object: " << httpUrl;
                    resData = ErrorResponse(500, type, "Response is not an object");
                } else {
                    resData["type"] = type;
                    const int code = resData.contains("code") && resData["code"].is_number_integer()
                        ? resData["code"].get<int>() : 200;
                    if (code >= 400) {
                        LOG_E << "HTTP request failed: " << httpUrl << ", code: " << code;
                    } else {
                        LOG_D << "HTTP request success: " << httpUrl;
                    }
                }
            } else {
                LOG_E << "Can't connect to main_app: " << httpUrl;
                resData = ErrorResponse(500, type, "Can't connect to main_app");
            }
            res = DumpJson(resData);
            return true;
        }

        if (!it->second.func) {
            LOG_E << "Message response not found: " << type;
            res = DumpJson(ErrorResponse(404, type, "Message response not found: " + type));
            return true;
        }
        bool result = it->second.func(data, resData);
        res = DumpJson(resData);
        return result;
    } catch (const std::exception &e) {
        LOG_E << "Message response failed: " << e.what();
        res = DumpJson(ErrorResponse(500, "error", e.what()));
        return true;
    }
}

void MqttResponse::RegisterMessageResponseHttp()
{
    // 应用与设置
    m_typeMap["AppConfig"] = {
        true,
        nullptr,
        "/api/app/config",
        "获取应用配置",
        "{}"
    };
    m_typeMap["SettingsInfo"] = {
        true,
        nullptr,
        "/api/settings/info",
        "获取设置信息",
        "{}"
    };

    // 采集
    m_typeMap["CaptureGet"] = {
        true,
        nullptr,
        "/api/capture/get",
        "获取采集配置",
        "{}"
    };
    m_typeMap["CaptureSet"] = {
        true,
        nullptr,
        "/api/capture/set",
        "设置采集配置",
        "{\"width\":0,\"height\":0,\"fps\":0}"
    };

    // 媒体
    m_typeMap["MediaList"] = {
        true,
        nullptr,
        "/api/media/list",
        "获取媒体文件列表",
        "{\"date\":\"\"}"
    };

    // 网络
    m_typeMap["NetworkWifiList"] = {
        true,
        nullptr,
        "/api/network/wifilist",
        "扫描 WiFi 列表",
        "{}"
    };
    m_typeMap["NetworkConnect"] = {
        true,
        nullptr,
        "/api/network/connect",
        "连接 WiFi",
        "{\"ssid\":\"\",\"pswd\":\"\"}"
    };
    m_typeMap["NetworkDefault"] = {
        true,
        nullptr,
        "/api/network/default",
        "获取默认网卡信息",
        "{}"
    };
    m_typeMap["NetworkAll"] = {
        true,
        nullptr,
        "/api/network/all",
        "获取所有网卡信息",
        "{}"
    };

    // 推流
    m_typeMap["StreamLive"] = {
        true,
        nullptr,
        "/api/stream/live",
        "直播流控制",
        "{\"mode\":\"get\",\"status\":false}"
    };
    m_typeMap["StreamReplay"] = {
        true,
        nullptr,
        "/api/stream/replay",
        "回放流控制",
        "{\"mode\":\"get\",\"status\":\"\",\"file\":\"\",\"seek\":0}"
    };
    m_typeMap["StreamSetUrl"] = {
        true,
        nullptr,
        "/api/stream/seturl",
        "设置回放流地址",
        "{\"url\":\"\"}"
    };

    // 系统
    m_typeMap["SystemReset"] = {
        true,
        nullptr,
        "/api/system/reset",
        "重启设备",
        "{}"
    };
    m_typeMap["SystemInfo"] = {
        true,
        nullptr,
        "/api/system/info",
        "获取设备信息",
        "{}"
    };
    m_typeMap["SystemMemory"] = {
        true,
        nullptr,
        "/api/system/memory",
        "获取内存使用情况",
        "{}"
    };
    m_typeMap["SystemDisk"] = {
        true,
        nullptr,
        "/api/system/disk",
        "获取磁盘使用情况",
        "{}"
    };
    m_typeMap["SystemCpu"] = {
        true,
        nullptr,
        "/api/system/cpu",
        "获取 CPU 信息",
        "{}"
    };
}
