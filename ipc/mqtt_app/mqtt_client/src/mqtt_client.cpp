#include <chrono>
#include <cstdint>
#include <ctime>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <nlohmann/json.hpp>
#include <SimpleIni.h>
#include "log.h"
#include "device_info.h"
#include "mqtt_response.h"
#include "mqtt_client.h"

void SleepUntil(
    std::chrono::steady_clock::time_point deadline,
    const std::atomic<bool> &running)
{
    while (running) {
        const auto now = std::chrono::steady_clock::now();
        if (now >= deadline) {
            return;
        }
        auto remain = deadline - now;
        if (remain > std::chrono::milliseconds(200)) {
            remain = std::chrono::milliseconds(200);
        }
        std::this_thread::sleep_for(remain);
    }
}

static bool ParseIccid(const std::string &response, std::string &iccid)
{
    const char *tags[] = {"+ICCID:", "+QCCID:"};
    for (const char *tag : tags) {
        const size_t begin = response.find(tag);
        if (begin == std::string::npos) {
            continue;
        }
        size_t value = begin + std::char_traits<char>::length(tag);
        while (value < response.size() && (response[value] == ' ' || response[value] == '\t')) {
            ++value;
        }
        const size_t end = response.find_first_of("\r\n", value);
        iccid = response.substr(value, end == std::string::npos ? std::string::npos : end - value);
        if (!iccid.empty()) {
            return true;
        }
    }
    return false;
}

MqttClient::MqttClient()
    : m_threadRunning(false)
    , m_client(nullptr)
    , m_mqttResponse(m_httpClient)
    , m_done(false)
    , m_rc(MQTTASYNC_SUCCESS)
{
    CSimpleIniA ini;
    SI_Error rcini = ini.LoadFile("/etc/mqtt_app/config.ini");
    if (rcini < 0) {
        LOG_E << "Failed to load mqtt config: /etc/mqtt_app/config.ini";
        return;
    }

    m_clientId = DeviceInfo::DeviceKey();

    m_server = ini.GetValue("mqtt", "server", "");
    m_topics = {ini.GetValue("mqtt", "topic", "") + m_clientId + "/svrtodev"};
    m_responseTopic = ini.GetValue("mqtt", "topic", "") + m_clientId + "/devtosvr";
    m_username = ini.GetValue("mqtt", "username", "");
    m_password = ini.GetValue("mqtt", "password", "");
    int httpPort = ini.GetLongValue("http", "port", 9000);
    m_diskSpaceWarnBytes = ini.GetLongValue("files", "spacewarn", 6 * SIZE_1MB * 1024);  // 6GB

    m_httpClient = HttpClient("http://127.0.0.1:" + std::to_string(httpPort));

    int rc = MQTTAsync_create(&m_client, m_server.c_str(), m_clientId.c_str(),
        MQTTCLIENT_PERSISTENCE_NONE, nullptr);
    if (rc != MQTTASYNC_SUCCESS) {
        LogError(rc, MQTTAsync_strerror(rc));
        m_client = nullptr;
        return;
    }

    rc = MQTTAsync_setCallbacks(m_client, this, ConnectionLost, MessageArrived, nullptr);
    if (rc != MQTTASYNC_SUCCESS) {
        LogError(rc, MQTTAsync_strerror(rc));
    }

    rc = MQTTAsync_setConnected(m_client, this, OnConnected);
    if (rc != MQTTASYNC_SUCCESS) {
        LogError(rc, MQTTAsync_strerror(rc));
    }

    m_threadRunning = true;
    m_checkSystemThread = std::thread(&MqttClient::CheckSystemThread, this);
}

MqttClient::~MqttClient()
{
    m_threadRunning = false;

    if (m_checkSystemThread.joinable()) {
        LOG_I << "Joining check system thread";
        m_checkSystemThread.join();
    }

    Disconnect();
    if (m_client) {
        MQTTAsync_destroy(&m_client);
    }
}

void MqttClient::PublishDiskSpaceWarning(long long freeBytes, long long warnBytes)
{
    nlohmann::json payload = {
        {"code", 200},
        {"type", "Warning"},
        {"data", {
            {"message", "Disk space is low"},
            {"free", freeBytes},
            {"warn", warnBytes}
        }}
    };
    PublishReply(payload.dump());
}

void MqttClient::CheckDiskSpace()
{
    nlohmann::json data = {
        {"token", ""}
    };

    httplib::Result result = m_httpClient.Post("/api/system/disk", data.dump());
    if (result && result->status == 200) {
        const std::string body = result->body;
        nlohmann::json resData = nlohmann::json::parse(body);
        const int code = (resData.contains("code") && resData["code"].is_number_integer())
            ? resData["code"].get<int>() : 0;
        if (code != 200) {
            LOG_E << "Failed to check disk space, code: " << code;
        } else {
            nlohmann::json diskData = resData["data"];
            if (diskData.is_object()) {
                const long long freeBytes = diskData["free"];
                LOG_I << "Disk space: free=" << freeBytes << " bytes";
                if (freeBytes <= m_diskSpaceWarnBytes) {
                    LOG_W << "Disk space is less than " << m_diskSpaceWarnBytes << " bytes";
                    PublishDiskSpaceWarning(freeBytes, m_diskSpaceWarnBytes);
                }
            } else {
                LOG_E << "Disk space data is not an object";
            }
        }
    } else {
        LOG_E << "Can't connect to main_app: /api/system/disk";
    }
}

void MqttClient::CheckSystemThread()
{
    static uint64_t timeCount = 0;
    while (m_threadRunning) {
        if (timeCount % 1440 == 0) {  // 每24小时检查一次磁盘空间
            CheckDiskSpace();
        }

        std::this_thread::sleep_for(std::chrono::seconds(60));
        timeCount += 1;
    }
}

int MqttClient::Connect()
{
    if (!m_client) {
        return -1;
    }

    MQTTAsync_connectOptions opts = MQTTAsync_connectOptions_initializer;
    opts.automaticReconnect = 1;
    opts.minRetryInterval = 1;
    opts.maxRetryInterval = 32;
    opts.keepAliveInterval = 20;
    opts.cleansession = 1;
    opts.username = m_username.c_str();
    opts.password = m_password.c_str();
    opts.onSuccess = OnConnectSuccess;
    opts.onFailure = OnConnectFailure;
    opts.context = this;

    BeginWait();
    LOG_I << "connecting to " << m_server;
    int rc = MQTTAsync_connect(m_client, &opts);
    if (rc != MQTTASYNC_SUCCESS) {
        LogError(rc, MQTTAsync_strerror(rc));
        return -1;
    }
    if (Wait() != 0) {
        return -1;
    }

    LOG_I << "connected";
    if (SubscribeTopics() != 0) {
        return -1;
    }
    return 0;
}

void MqttClient::Disconnect()
{
    if (!m_client || !MQTTAsync_isConnected(m_client)) {
        return;
    }

    MQTTAsync_disconnectOptions opts = MQTTAsync_disconnectOptions_initializer;
    opts.onSuccess = OnCommandSuccess;
    opts.onFailure = OnCommandFailure;
    opts.context = this;

    BeginWait();
    int rc = MQTTAsync_disconnect(m_client, &opts);
    if (rc != MQTTASYNC_SUCCESS) {
        LogError(rc, MQTTAsync_strerror(rc));
        return;
    }
    if (Wait() == 0) {
        LOG_I << "disconnected";
    }
}

int MqttClient::SubscribeTopics()
{
    for (const std::string &topic : m_topics) {
        if (Subscribe(topic, 1) != 0) {
            return -1;
        }
    }
    return 0;
}

int MqttClient::Subscribe(const std::string &topic, int qos)
{
    MQTTAsync_responseOptions opts = MQTTAsync_responseOptions_initializer;
    opts.onSuccess = OnCommandSuccess;
    opts.onFailure = OnCommandFailure;
    opts.context = this;

    BeginWait();
    int rc = MQTTAsync_subscribe(m_client, topic.c_str(), qos, &opts);
    if (rc != MQTTASYNC_SUCCESS) {
        LogError(rc, MQTTAsync_strerror(rc));
        return -1;
    }
    if (Wait() != 0) {
        return -1;
    }

    LOG_I << "subscribed " << topic << " qos " << qos;
    return 0;
}

int MqttClient::Publish(const std::string &topic, const std::string &payload, int qos)
{
    MQTTAsync_message msg = MQTTAsync_message_initializer;
    msg.payload = const_cast<char *>(payload.data());
    msg.payloadlen = static_cast<int>(payload.size());
    msg.qos = qos;
    msg.retained = 0;

    MQTTAsync_responseOptions opts = MQTTAsync_responseOptions_initializer;
    opts.onSuccess = OnCommandSuccess;
    opts.onFailure = OnCommandFailure;
    opts.context = this;

    BeginWait();
    int rc = MQTTAsync_sendMessage(m_client, topic.c_str(), &msg, &opts);
    if (rc != MQTTASYNC_SUCCESS) {
        LogError(rc, MQTTAsync_strerror(rc));
        LOG_E << "Publish " << topic << " failed: " << MQTTAsync_strerror(rc);
        return -1;
    }
    if (Wait() != 0) {
        LOG_E << "Publish " << topic << " failed: " << MQTTAsync_strerror(rc);
        return -1;
    }

    LOG_D << "published to " << topic;
    return 0;
}

void MqttClient::OnMessage(const std::string &topic, const std::string &payload)
{
    LOG_D << topic << ": " << payload;
    std::string response;
    if (!m_mqttResponse.MessageResponse(payload, response)) {
        LOG_D << "Message response not need to reply";
        return;
    }
    PublishReply(response);
}

void MqttClient::PublishReply(const std::string &payload)
{
    MQTTAsync_message msg = MQTTAsync_message_initializer;
    msg.payload = const_cast<char *>(payload.data());
    msg.payloadlen = static_cast<int>(payload.size());
    msg.qos = 1;
    msg.retained = 0;

    MQTTAsync_responseOptions opts = MQTTAsync_responseOptions_initializer;
    opts.onSuccess = OnPublishSuccess;
    opts.onFailure = OnPublishFailure;
    opts.context = this;

    const int rc = MQTTAsync_sendMessage(m_client, m_responseTopic.c_str(), &msg, &opts);
    if (rc != MQTTASYNC_SUCCESS) {
        LOG_E << "Publish " << m_responseTopic << " failed: " << MQTTAsync_strerror(rc);
        return;
    }
    LOG_D << "reply queued: " << m_responseTopic << " " << payload;
}

void MqttClient::OnPublishSuccess(void *context, MQTTAsync_successData *response)
{
    (void)response;
    auto *self = static_cast<MqttClient *>(context);
    LOG_D << "published to " << self->m_responseTopic;
}

void MqttClient::OnPublishFailure(void *context, MQTTAsync_failureData *response)
{
    auto *self = static_cast<MqttClient *>(context);
    const int rc = response ? response->code : MQTTASYNC_FAILURE;
    const char *message = (response && response->message) ? response->message : MQTTAsync_strerror(rc);
    LOG_E << "Publish " << self->m_responseTopic << " failed: " << message;
}

void MqttClient::OnConnected(void *context, char *cause)
{
    auto *self = static_cast<MqttClient *>(context);
    if (cause != nullptr && cause[0] != '\0') {
        LOG_I << "MQTT reconnected: " << cause;
    } else {
        LOG_I << "MQTT reconnected";
    }
    self->Resubscribe();
}

void MqttClient::Resubscribe()
{
    for (const std::string &topic : m_topics) {
        MQTTAsync_responseOptions opts = MQTTAsync_responseOptions_initializer;
        opts.onSuccess = OnResubscribeSuccess;
        opts.onFailure = OnResubscribeFailure;
        opts.context = this;
        const int rc = MQTTAsync_subscribe(m_client, topic.c_str(), 1, &opts);
        if (rc != MQTTASYNC_SUCCESS) {
            LOG_E << "Resubscribe " << topic << " failed: " << MQTTAsync_strerror(rc);
        }
    }
}

void MqttClient::OnResubscribeSuccess(void *context, MQTTAsync_successData *response)
{
    (void)context;
    const int qos = response ? response->alt.qos : -1;
    LOG_I << "resubscribed qos " << qos;
}

void MqttClient::OnResubscribeFailure(void *context, MQTTAsync_failureData *response)
{
    (void)context;
    const int rc = response ? response->code : MQTTASYNC_FAILURE;
    const char *message = (response && response->message) ? response->message : MQTTAsync_strerror(rc);
    LOG_E << "Resubscribe failed: " << message;
}

void MqttClient::OnConnectionLost(const std::string &cause)
{
    LOG_W << "connection lost: " << cause;
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_done) {
        m_rc = MQTTASYNC_DISCONNECTED;
        m_error = cause.empty() ? "connection lost" : cause;
        m_done = true;
        m_cv.notify_one();
    }
}

void MqttClient::BeginWait()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_done = false;
    m_rc = MQTTASYNC_FAILURE;
    m_error.clear();
}

int MqttClient::Wait()
{
    std::unique_lock<std::mutex> lock(m_mutex);
    m_cv.wait(lock, [this] { return m_done; });
    if (m_rc != MQTTASYNC_SUCCESS) {
        LogError(m_rc, m_error.c_str());
        return -1;
    }
    return 0;
}

void MqttClient::Finish(int rc, const char *message)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_rc = rc;
    m_error = message ? message : "";
    m_done = true;
    m_cv.notify_one();
}

void MqttClient::LogError(int rc, const char *message) const
{
    const char *text = (message && message[0] != '\0') ? message : MQTTAsync_strerror(rc);
    LOG_E << "MQTT error [" << rc << "]: " << text;
}

void MqttClient::OnConnectSuccess(void *context, MQTTAsync_successData *response)
{
    (void)response;
    static_cast<MqttClient *>(context)->Finish(MQTTASYNC_SUCCESS, nullptr);
}

void MqttClient::OnConnectFailure(void *context, MQTTAsync_failureData *response)
{
    int rc = response ? response->code : MQTTASYNC_FAILURE;
    const char *message = (response && response->message) ? response->message : nullptr;
    static_cast<MqttClient *>(context)->Finish(rc, message);
}

void MqttClient::OnCommandSuccess(void *context, MQTTAsync_successData *response)
{
    (void)response;
    static_cast<MqttClient *>(context)->Finish(MQTTASYNC_SUCCESS, nullptr);
}

void MqttClient::OnCommandFailure(void *context, MQTTAsync_failureData *response)
{
    int rc = response ? response->code : MQTTASYNC_FAILURE;
    const char *message = (response && response->message) ? response->message : nullptr;
    static_cast<MqttClient *>(context)->Finish(rc, message);
}

void MqttClient::ConnectionLost(void *context, char *cause)
{
    static_cast<MqttClient *>(context)->OnConnectionLost(cause ? cause : "");
}

int MqttClient::MessageArrived(void *context, char *topicName, int topicLen, MQTTAsync_message *message)
{
    std::string topic;
    if (topicName) {
        if (topicLen > 0) {
            topic.assign(topicName, static_cast<size_t>(topicLen));
        } else {
            topic.assign(topicName);
        }
    }

    std::string payload;
    if (message && message->payload && message->payloadlen > 0) {
        payload.assign(static_cast<const char *>(message->payload),
            static_cast<size_t>(message->payloadlen));
    }

    try {
        static_cast<MqttClient *>(context)->OnMessage(topic, payload);
    } catch (const std::exception &e) {
        LOG_E << "MQTT message failed: " << e.what();
    } catch (...) {
        LOG_E << "MQTT message failed";
    }
    MQTTAsync_freeMessage(&message);
    MQTTAsync_free(topicName);
    return 1;
}
