#pragma once

#include <string>
#include <nlohmann/json.hpp>
#include "http_client.h"

typedef std::function<bool(const nlohmann::json &reqData, nlohmann::json &res)> MessageResponseFunc;

class MqttResponse {
public:
    explicit MqttResponse(HttpClient &httpClient);
    ~MqttResponse();

    // 返回true表示需要回复，否则不需要回复
    bool MessageResponse(const std::string &req, std::string &res);

private:
    struct TypeInfo {
        bool isHttp;
        MessageResponseFunc func;
        std::string httpUrl;
        std::string description;
        std::string parameters;
    };

    void RegisterMessageResponseHttp();

    HttpClient &m_httpClient;

    std::map<std::string, TypeInfo> m_typeMap;
};
