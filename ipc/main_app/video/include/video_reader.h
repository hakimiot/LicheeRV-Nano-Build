#pragma once

#include <string>
#include <thread>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <cstdint>

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
}

#include "rtmp_streamer.h"

class VideoReader {
public:
    enum class State {
        Idle,       // 未打开 / 已停止
        Playing,
        Paused,
        Stopped
    };

    VideoReader();
    ~VideoReader();

    // 打开视频文件 & 读取同名 json（或指定 json 路径）
    // jsonPath 为空时，默认读取 <videoPath>.json
    bool Open(const std::string& videoPath, const std::string& jsonPath = "");

    // 关闭并释放所有资源
    void Close();

    // 播放控制
    bool Play();       // 开始 / 从暂停恢复
    void Pause();      // 暂停（队列里的帧继续发完）
    void Stop();       // 停止并复位到开头

    // 设置进度（毫秒），内部会 seek 到最近的关键帧
    bool Seek(int64_t ms);

    // 绑定推流器
    void SetStreamer(RTMPStreamer* s) { m_streamer = s; }

    // 状态查询
    State   GetState()      const { return m_state.load(); }
    std::string GetStateString();
    int64_t GetDurationMs() const { return m_durationMs; }
    int64_t GetPositionMs() const { return m_positionMs.load(); }
    bool    IsOpened()      const { return m_fmtCtx != nullptr; }

private:
    void ReadLoop();
    bool SeekInternal(int64_t ms);
    bool LoadDurationFromJson(const std::string& jsonPath);

    RTMPStreamer* m_streamer = nullptr;

    // FFmpeg
    AVFormatContext*   m_fmtCtx     = nullptr;
    AVCodecParameters* m_codecPar   = nullptr;
    int                m_videoIndex = -1;
    AVPacket*          m_pkt        = nullptr;

    // 线程
    std::thread             m_readThread;
    std::atomic<bool>       m_threadRunning{false};
    std::atomic<State>      m_state{State::Idle};

    // 暂停 / 唤醒
    std::mutex              m_pauseMutex;
    std::condition_variable m_pauseCv;

    // Seek 请求
    std::atomic<bool>       m_seekRequested{false};
    std::atomic<int64_t>    m_seekTargetMs{0};
    std::mutex              m_seekMutex;    // 保护 fmtCtx

    // 时长 & 进度
    int64_t                 m_durationMs = 0;
    std::atomic<int64_t>    m_positionMs{0};

    // 视频信息
    std::string             m_path;
    int                     m_fps = 30;
};
