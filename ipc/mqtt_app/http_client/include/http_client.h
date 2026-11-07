#pragma once

#include <httplib.h>

class HttpClient {
public:
    HttpClient(const std::string &url = "http://127.0.0.1:9000");
    HttpClient(HttpClient &&) = default;
    HttpClient &operator=(HttpClient &&) = default;
    ~HttpClient();

    httplib::Result Post(const std::string &url, const std::string &payload);

private:
    httplib::Client m_httpClient;
};
