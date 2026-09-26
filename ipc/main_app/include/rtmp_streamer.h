#pragma once

#include <string>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <atomic>
#include <cstdint>
#include <vector>
#include <chrono>

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/avutil.h>
}

// H.264 码流包
struct H264Packet {
    std::vector<uint8_t> data;
};

class RTMPStreamer {
public:
    RTMPStreamer();
    ~RTMPStreamer();

    // 初始化，设置 RTMP 地址、分辨率、帧率
    bool Init(const std::string& url, int width, int height, int fps);
    void Start();
    void Stop();

    // 推流 H.264 码流（由 VENC 输出）
    void PushH264(const uint8_t* data, size_t len);

    // 状态查询
    bool IsConnected() const { return m_connected; }
    void SetRtmpUrl(const std::string& url);
    std::string GetRtmpUrl() const { return m_url; }
    void SetResolution(int width, int height);
    void SetFps(int fps);

private:
    bool InitInternal();
    bool ReinitRtmp();
    void Cleanup();
    void StreamLoop();
    bool ExtractSpsPps(const uint8_t* data, size_t len);
    bool IsKeyFrame(const uint8_t* data, size_t len);
    bool FindStartCode(const uint8_t* data, size_t size, size_t pos, size_t& startCodeSize);

    // RTMP 配置
    std::string m_url;
    int m_width;
    int m_height;
    int m_fps;

    // FFmpeg 资源
    AVFormatContext* m_formatCtx = nullptr;
    AVStream* m_stream = nullptr;
    AVPacket* m_packet = nullptr;

    // H.264 解析器（自动提取 SPS/PPS）
    AVCodecParserContext* m_parser = nullptr;
    AVCodecContext* m_parserCtx = nullptr;

    // 时间戳
    int64_t m_pts = 0;

    // 码流队列
    std::queue<H264Packet> m_queue;
    std::mutex m_queueMutex;
    std::condition_variable m_queueCv;
    size_t m_maxQueueSize = 30;

    // 线程
    std::thread m_streamThread;
    std::atomic<bool> m_running{false};
    std::atomic<bool> m_connected{false};

    // 重连
    std::atomic<bool> m_reconnectRequested{false};
    std::chrono::steady_clock::time_point m_lastReconnectTime;
    int m_reconnectCount = 0;

    // SPS/PPS 缓存
    std::vector<uint8_t> m_cachedSps;
    std::vector<uint8_t> m_cachedPps;

    // FLV header 是否已写
    bool m_headerWritten = false;
    bool m_extradataCopied = false;
};
