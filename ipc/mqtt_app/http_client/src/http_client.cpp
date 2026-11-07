#include "log.h"
#include "http_client.h"

HttpClient::HttpClient(const std::string &url)
    : m_httpClient(url)
{
}

HttpClient::~HttpClient()
{
}

httplib::Result HttpClient::Post(const std::string &url, const std::string &payload)
{
    httplib::Headers headers = {
        { "Content-Type", "application/json" },
        { "Accept-Encoding", "identity" }
    };

    httplib::Result result = m_httpClient.Post(url, headers, payload, "application/json");
    if (result.error() != httplib::Error::Success) {
        LOG_E << "Failed to post to http server: " << result.error();
        return result;
    }
    if (!result || result->status != 200) {
        LOG_E << "Failed to post to http server: " << (result ? result->status : -1);
        return result;
    }
    return result;
}
