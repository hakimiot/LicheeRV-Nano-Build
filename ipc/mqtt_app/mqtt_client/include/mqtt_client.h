#pragma once

#include <MQTTAsync.h>
#include <condition_variable>
#include <mutex>
#include <string>
#include <vector>
#include "http_client.h"
#include "mqtt_response.h"

class MqttClient {
public:
    MqttClient();
    ~MqttClient();

    int Connect();
    void Disconnect();
    int Publish(const std::string &topic, const std::string &payload, int qos);
    void PublishReply(const std::string &payload);

private:
    void PublishDiskSpaceWarning(long long freeBytes, long long warnBytes);

    void CheckDiskSpace();

    void CheckSystemThread();

    int Subscribe(const std::string &topic, int qos);
    int SubscribeTopics();
    void BeginWait();
    int Wait();
    void Finish(int rc, const char *message);
    void LogError(int rc, const char *message) const;
    void OnMessage(const std::string &topic, const std::string &payload);
    void OnConnectionLost(const std::string &cause);
    void Resubscribe();

    static void OnConnectSuccess(void *context, MQTTAsync_successData *response);
    static void OnConnectFailure(void *context, MQTTAsync_failureData *response);
    static void OnCommandSuccess(void *context, MQTTAsync_successData *response);
    static void OnCommandFailure(void *context, MQTTAsync_failureData *response);
    static void OnPublishSuccess(void *context, MQTTAsync_successData *response);
    static void OnPublishFailure(void *context, MQTTAsync_failureData *response);
    static void ConnectionLost(void *context, char *cause);
    static void OnConnected(void *context, char *cause);
    static void OnResubscribeSuccess(void *context, MQTTAsync_successData *response);
    static void OnResubscribeFailure(void *context, MQTTAsync_failureData *response);
    static int MessageArrived(void *context, char *topicName, int topicLen, MQTTAsync_message *message);

    std::thread m_checkSystemThread;
    std::atomic<bool> m_threadRunning;

    std::string m_server;
    std::string m_clientId;
    std::string m_username;
    std::string m_password;
    std::vector<std::string> m_topics;
    std::string m_responseTopic;

    MQTTAsync m_client;
    HttpClient m_httpClient;
    MqttResponse m_mqttResponse;

    std::mutex m_mutex;
    std::condition_variable m_cv;
    bool m_done;
    int m_rc;
    std::string m_error;

    long long m_diskSpaceWarnBytes = 0;
};
