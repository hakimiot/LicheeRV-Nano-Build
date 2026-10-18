#pragma once

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <string>
#include <thread>

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/codec_par.h>
}

class RTMPStreamer;

class VideoReader
{
public:
    enum class State {
        Idle,
        Playing,
        Paused,
        Stopped
    };

    VideoReader();
    ~VideoReader();

    VideoReader(const VideoReader&) = delete;
    VideoReader& operator=(const VideoReader&) = delete;

    bool Open(const std::string& videoPath,
              const std::string& jsonPath = "");

    void Close();

    bool Play();
    void Pause();
    void Stop();
    bool Seek(int64_t ms);

    void SetStreamer(RTMPStreamer* streamer)
    {
        m_streamer = streamer;
    }

    State GetState() const
    {
        return m_state.load();
    }

    std::string GetStateString() const;

    int64_t GetDurationMs() const
    {
        return m_durationMs.load();
    }

    int64_t GetPositionMs() const
    {
        return m_positionMs.load();
    }

    bool IsOpened() const
    {
        return m_fmtCtx != nullptr;
    }

private:
    bool LoadDurationFromJson(const std::string& jsonPath);

    // 将 FFmpeg 的输入位置移动到估算的文件偏移。
    bool SeekInternal(int64_t ms);

    // 播放线程。
    void ReadLoop();

    // 将毫秒转换成估算的文件偏移。
    int64_t EstimateFileOffset(int64_t ms) const;

    // 播放过程中等待跳转目标处的关键帧。
    bool ShouldSkipPacket(const AVPacket* pkt);

    AVFormatContext* m_fmtCtx = nullptr;
    AVPacket* m_pkt = nullptr;
    AVCodecParameters* m_codecPar = nullptr;

    int m_videoIndex = -1;

    std::string m_path;
    int64_t m_fileSize = 0;

    std::atomic<int64_t> m_durationMs{0};
    std::atomic<int64_t> m_positionMs{0};
    std::atomic<int> m_fps{30};

    std::atomic<State> m_state{State::Idle};
    std::atomic<bool> m_threadRunning{false};

    std::thread m_readThread;

    std::mutex m_pauseMutex;
    std::condition_variable m_pauseCv;

    // 只保护 FFmpeg demuxer 的 seek/read 操作。
    std::mutex m_seekMutex;

    std::atomic<bool> m_seekRequested{false};
    std::atomic<int64_t> m_seekTargetMs{0};

    // 跳转后等待目标位置附近的关键帧。
    std::atomic<bool> m_waitForKeyframe{false};
    std::atomic<int64_t> m_seekTargetOffset{0};

    RTMPStreamer* m_streamer = nullptr;
};
