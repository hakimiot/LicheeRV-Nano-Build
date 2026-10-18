#include "video_reader.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <fstream>
#include <sys/stat.h>

#include <nlohmann/json.hpp>

extern "C" {
#include <libavutil/error.h>
#include <libavutil/mathematics.h>
}

#include "mainapp_log.h"
#include "rtmp_streamer.h"

namespace {

constexpr int64_t kSeekBackBytes = 2 * 1024 * 1024;

static std::string AvErrorToString(int err)
{
    char buffer[AV_ERROR_MAX_STRING_SIZE] = {};
    av_strerror(err, buffer, sizeof(buffer));
    return buffer;
}

} // namespace

// ============================================================
// 构造 / 析构
// ============================================================

VideoReader::VideoReader() = default;

VideoReader::~VideoReader()
{
    Close();
}

// ============================================================
// 打开文件
// ============================================================

bool VideoReader::Open(const std::string& videoPath,
                       const std::string& jsonPath)
{
    Close();

    m_path = videoPath;

    struct stat st {};
    if (::stat(videoPath.c_str(), &st) != 0 || st.st_size <= 0) {
        LOG_E << "Cannot stat video file: " << videoPath;
        return false;
    }

    m_fileSize = static_cast<int64_t>(st.st_size);

    int ret = avformat_open_input(
        &m_fmtCtx, videoPath.c_str(), nullptr, nullptr);

    if (ret < 0) {
        LOG_E << "avformat_open_input failed: "
              << AvErrorToString(ret);
        Close();
        return false;
    }

    ret = avformat_find_stream_info(m_fmtCtx, nullptr);
    if (ret < 0) {
        LOG_E << "avformat_find_stream_info failed: "
              << AvErrorToString(ret);
        Close();
        return false;
    }

    for (unsigned int i = 0; i < m_fmtCtx->nb_streams; ++i) {
        AVStream* stream = m_fmtCtx->streams[i];

        if (stream->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            m_videoIndex = static_cast<int>(i);
            break;
        }
    }

    if (m_videoIndex < 0) {
        LOG_E << "No video stream found";
        Close();
        return false;
    }

    AVStream* stream = m_fmtCtx->streams[m_videoIndex];

    if (stream->codecpar->codec_id != AV_CODEC_ID_H264) {
        LOG_E << "Not an H.264 stream";
        Close();
        return false;
    }

    m_codecPar = stream->codecpar;

    AVRational frameRate =
        av_guess_frame_rate(m_fmtCtx, stream, nullptr);

    if (frameRate.num > 0 && frameRate.den > 0) {
        const int fps = static_cast<int>(
            static_cast<double>(frameRate.num) /
            frameRate.den + 0.5);

        if (fps > 0) {
            m_fps = fps;
        }
    }

    m_pkt = av_packet_alloc();

    if (!m_pkt) {
        LOG_E << "av_packet_alloc failed";
        Close();
        return false;
    }

    const std::string metadataPath =
        jsonPath.empty() ? videoPath + ".json" : jsonPath;

    if (!LoadDurationFromJson(metadataPath)) {
        LOG_W << "Cannot load duration/fps from JSON: "
              << metadataPath;
        m_durationMs = 0;
    }

    /*
     * avformat_find_stream_info() 可能读取一部分数据。
     * 尝试把裸码流输入位置恢复到文件开头。
     */
    if (m_fmtCtx->pb) {
        const int64_t seekRet = avio_seek(
            m_fmtCtx->pb, 0, SEEK_SET);

        if (seekRet >= 0) {
            avformat_flush(m_fmtCtx);
        } else {
            LOG_W << "Cannot rewind input after stream info: "
                  << seekRet;
        }
    }

    m_positionMs = 0;
    m_seekRequested = false;
    m_waitForKeyframe = false;
    m_seekTargetOffset = 0;
    m_state = State::Idle;

    LOG_I << "VideoReader opened: " << videoPath
          << " fileSize=" << m_fileSize
          << " duration=" << m_durationMs.load() << "ms"
          << " fps=" << m_fps.load()
          << " resolution="
          << m_codecPar->width << "x" << m_codecPar->height;

    return true;
}

// ============================================================
// 从 JSON 读取时长和帧率
// ============================================================

bool VideoReader::LoadDurationFromJson(const std::string& jsonPath)
{
    std::ifstream ifs(jsonPath);

    if (!ifs.is_open()) {
        LOG_W << "Cannot open JSON: " << jsonPath;
        return false;
    }

    nlohmann::json j;

    try {
        ifs >> j;
    } catch (const std::exception& e) {
        LOG_E << "JSON parse error: " << e.what();
        return false;
    }

    if (j.contains("duration_ms") &&
        j["duration_ms"].is_number()) {

        m_durationMs = j["duration_ms"].get<int64_t>();

    } else if (j.contains("durationMs") &&
               j["durationMs"].is_number()) {

        m_durationMs = j["durationMs"].get<int64_t>();

    } else if (j.contains("duration_sec") &&
               j["duration_sec"].is_number()) {

        const double seconds =
            j["duration_sec"].get<double>();

        m_durationMs =
            static_cast<int64_t>(seconds * 1000.0);

    } else if (j.contains("duration") &&
               j["duration"].is_number()) {

        const double value =
            j["duration"].get<double>();

        bool isSeconds = false;

        if (j.contains("unit") && j["unit"].is_string()) {
            const std::string unit =
                j["unit"].get<std::string>();

            isSeconds =
                unit == "s" ||
                unit == "sec" ||
                unit == "second" ||
                unit == "seconds";
        }

        m_durationMs = isSeconds
            ? static_cast<int64_t>(value * 1000.0)
            : static_cast<int64_t>(value);

    } else {
        LOG_W << "No duration field in JSON: " << jsonPath;
        return false;
    }

    if (m_durationMs < 0) {
        m_durationMs = 0;
    }

    if (j.contains("fps") && j["fps"].is_number()) {
        const int fps =
            static_cast<int>(j["fps"].get<double>() + 0.5);

        if (fps > 0) {
            m_fps = fps;
        }
    }

    LOG_I << "Loaded metadata: duration="
          << m_durationMs.load()
          << "ms fps=" << m_fps.load();

    return true;
}

// ============================================================
// 关闭
// ============================================================

void VideoReader::Close()
{
    Stop();

    if (m_pkt) {
        av_packet_free(&m_pkt);
    }

    if (m_fmtCtx) {
        avformat_close_input(&m_fmtCtx);
        m_fmtCtx = nullptr;
    }

    m_codecPar = nullptr;
    m_videoIndex = -1;

    m_fileSize = 0;
    m_path.clear();

    m_durationMs = 0;
    m_positionMs = 0;
    m_fps = 30;

    m_seekRequested = false;
    m_waitForKeyframe = false;
    m_seekTargetOffset = 0;
    m_state = State::Idle;
}

// ============================================================
// 播放
// ============================================================

bool VideoReader::Play()
{
    if (!m_fmtCtx || !m_pkt) {
        LOG_E << "Play failed: video is not opened";
        return false;
    }

    const State state = m_state.load();

    if (state == State::Playing) {
        return true;
    }

    if (state == State::Paused) {
        m_state = State::Playing;
        m_pauseCv.notify_all();

        LOG_I << "Resume playback at "
              << m_positionMs.load() << "ms";

        return true;
    }

    if (m_threadRunning.load()) {
        return true;
    }

    if (m_readThread.joinable()) {
        m_readThread.join();
    }

    m_threadRunning = true;
    m_state = State::Playing;

    try {
        m_readThread = std::thread(
            &VideoReader::ReadLoop, this);
    } catch (const std::exception& e) {
        m_threadRunning = false;
        m_state = State::Idle;

        LOG_E << "Create read thread failed: " << e.what();
        return false;
    }

    return true;
}

// ============================================================
// 暂停
// ============================================================

void VideoReader::Pause()
{
    if (m_state.load() != State::Playing) {
        return;
    }

    m_state = State::Paused;
    m_pauseCv.notify_all();

    LOG_I << "Paused at " << m_positionMs.load() << "ms";
}

// ============================================================
// 停止
// ============================================================

void VideoReader::Stop()
{
    /*
     * 先唤醒线程，再等待线程退出。
     * 不调用 av_seek_frame()，也不等待整个推流队列清空。
     */
    m_state = State::Stopped;
    m_threadRunning = false;
    m_seekRequested = false;

    m_pauseCv.notify_all();

    if (m_readThread.joinable()) {
        if (m_readThread.get_id() ==
            std::this_thread::get_id()) {
            LOG_E << "Stop called from read thread; cannot self-join";
            return;
        }

        m_readThread.join();
    }

    /*
     * m_streamer 应当指向回放专用的 RTMPStreamer 实例。
     * 如果它与直播共用，ClearQueue() 会清除该实例的待发送数据。
     */
    if (m_streamer) {
        m_streamer->ClearQueue();
    }

    if (m_fmtCtx && m_fmtCtx->pb) {
        /*
         * 重新定位到文件开头，下一次 Play 从头播放。
         * 裸 H.264 使用字节定位，不调用 av_seek_frame()。
         */
        std::lock_guard<std::mutex> lock(m_seekMutex);

        const int64_t ret =
            avio_seek(m_fmtCtx->pb, 0, SEEK_SET);

        if (ret >= 0) {
            avformat_flush(m_fmtCtx);
            m_waitForKeyframe = false;
            m_seekTargetOffset = 0;
        } else {
            LOG_W << "Rewind after Stop failed: " << ret;
        }
    }

    m_positionMs = 0;
    m_state = State::Idle;

    LOG_I << "Stopped";
}

// ============================================================
// 根据时长和文件大小估算字节偏移
// ============================================================

int64_t VideoReader::EstimateFileOffset(int64_t ms) const
{
    if (m_fileSize <= 0 ||
        m_durationMs.load() <= 0 ||
        ms <= 0) {
        return 0;
    }

    if (ms >= m_durationMs.load()) {
        return std::max<int64_t>(0, m_fileSize - 1);
    }

    const long double ratio =
        static_cast<long double>(ms) /
        static_cast<long double>(m_durationMs.load());

    int64_t offset = static_cast<int64_t>(
        ratio * static_cast<long double>(m_fileSize));

    offset = std::max<int64_t>(0, offset);
    offset = std::min<int64_t>(offset, m_fileSize - 1);

    return offset;
}

// ============================================================
// 设置进度
// ============================================================

bool VideoReader::Seek(int64_t ms)
{
    if (!m_fmtCtx || !m_pkt) {
        LOG_E << "Seek failed: video is not opened";
        return false;
    }

    if (ms < 0) {
        ms = 0;
    }

    const int64_t duration = m_durationMs.load();

    if (duration > 0 && ms > duration) {
        ms = duration;
    }

    m_seekTargetMs = ms;

    if (m_threadRunning.load()) {
        /*
         * 只提交请求，由播放线程自己执行 seek。
         * 不在 HTTP 请求线程中等待读取锁或扫描码流。
         */
        m_seekRequested = true;
        m_pauseCv.notify_all();

        LOG_I << "Seek requested: " << ms << "ms";
        return true;
    }

    return SeekInternal(ms);
}

// ============================================================
// 字节定位，不使用 av_seek_frame
// ============================================================

bool VideoReader::SeekInternal(int64_t ms)
{
    if (!m_fmtCtx || !m_fmtCtx->pb || m_fileSize <= 0) {
        LOG_E << "SeekInternal failed: invalid input";
        return false;
    }

    const int64_t targetOffset = EstimateFileOffset(ms);

    /*
     * 从目标位置前面一小段开始扫描。
     * 这样可避免从任意 NAL 中间开始后，错过目标附近的起始码。
     *
     * 最终只发送目标偏移处或之后的关键帧。
     */
    const int64_t scanOffset =
        std::max<int64_t>(0, targetOffset - kSeekBackBytes);

    std::lock_guard<std::mutex> lock(m_seekMutex);

    const int64_t ret = avio_seek(
        m_fmtCtx->pb, scanOffset, SEEK_SET);

    if (ret < 0) {
        LOG_E << "avio_seek failed: "
              << AvErrorToString(static_cast<int>(ret));
        return false;
    }

    avformat_flush(m_fmtCtx);

    m_seekTargetOffset = targetOffset;
    m_waitForKeyframe = (targetOffset > 0);
    m_positionMs = ms;

    /*
     * 清除旧回放数据，防止跳转前的帧继续发送。
     * 此处必须使用回放专用 streamer，不能与直播共用队列。
     */
    if (m_streamer) {
        m_streamer->ClearQueue();
    }

    LOG_I << "Seek positioned: target=" << ms
          << "ms, targetOffset=" << targetOffset
          << ", scanOffset=" << scanOffset;

    return true;
}

// ============================================================
// 跳转后丢弃目标关键帧之前的包
// ============================================================

bool VideoReader::ShouldSkipPacket(const AVPacket* pkt)
{
    if (!pkt || pkt->stream_index != m_videoIndex) {
        return true;
    }

    if (!m_waitForKeyframe.load()) {
        return false;
    }

    /*
     * 需要同时满足：
     * 1. FFmpeg 将该包识别为关键帧；
     * 2. 包的位置在目标偏移之后。
     *
     * 对无法提供 pos 的 demuxer，只能退化为遇到的第一个关键帧。
     */
    const bool isKeyFrame =
        (pkt->flags & AV_PKT_FLAG_KEY) != 0;

    if (!isKeyFrame) {
        return true;
    }

    const int64_t targetOffset = m_seekTargetOffset.load();

    if (pkt->pos >= 0 && pkt->pos < targetOffset) {
        return true;
    }

    m_waitForKeyframe = false;

    LOG_I << "Seek keyframe found: packetPos="
          << pkt->pos << ", targetOffset="
          << targetOffset;

    return false;
}

// ============================================================
// 状态字符串
// ============================================================

std::string VideoReader::GetStateString() const
{
    switch (m_state.load()) {
    case State::Idle:
        return "idle";

    case State::Playing:
        return "playing";

    case State::Paused:
        return "paused";

    case State::Stopped:
        return "stopped";
    }

    return "unknown";
}

// ============================================================
// 播放线程
// ============================================================

void VideoReader::ReadLoop()
{
    LOG_I << "VideoReader ReadLoop start";

    const int fps = std::max(1, m_fps.load());

    const auto frameDuration =
        std::chrono::microseconds(1000000 / fps);

    auto nextFrameTime = std::chrono::steady_clock::now();

    while (m_threadRunning.load()) {

        // ----------------------------------------------------
        // 1. 暂停等待：支持停止和跳转唤醒
        // ----------------------------------------------------

        {
            std::unique_lock<std::mutex> lock(m_pauseMutex);

            m_pauseCv.wait(lock, [this] {
                return !m_threadRunning.load() ||
                       m_state.load() != State::Paused ||
                       m_seekRequested.load();
            });
        }

        if (!m_threadRunning.load() ||
            m_state.load() == State::Stopped) {
            break;
        }

        // ----------------------------------------------------
        // 2. 处理跳转请求
        // ----------------------------------------------------

        if (m_seekRequested.exchange(false)) {
            const int64_t target = m_seekTargetMs.load();

            if (!SeekInternal(target)) {
                LOG_E << "Seek failed: target=" << target << "ms";
            }

            nextFrameTime = std::chrono::steady_clock::now();

            /*
             * 暂停状态下允许修改进度，但不继续发送视频。
             */
            if (m_state.load() == State::Paused) {
                continue;
            }
        }

        if (!m_threadRunning.load()) {
            break;
        }

        if (m_state.load() != State::Playing) {
            continue;
        }

        // ----------------------------------------------------
        // 3. 读取 H.264 packet
        // ----------------------------------------------------

        int ret = 0;

        {
            std::lock_guard<std::mutex> lock(m_seekMutex);

            if (!m_threadRunning.load()) {
                break;
            }

            ret = av_read_frame(m_fmtCtx, m_pkt);
        }

        if (ret < 0) {
            if (ret == AVERROR_EOF) {
                LOG_I << "VideoReader reached EOF";
            } else {
                LOG_E << "av_read_frame failed: "
                      << AvErrorToString(ret);
            }

            /*
             * 不等待 RTMP 队列清空，立即结束回放线程。
             */
            m_threadRunning = false;

            if (m_state.load() != State::Stopped) {
                m_state = State::Idle;
            }

            break;
        }

        // ----------------------------------------------------
        // 4. 丢弃非视频包及跳转目标之前的包
        // ----------------------------------------------------

        if (ShouldSkipPacket(m_pkt)) {
            av_packet_unref(m_pkt);
            continue;
        }

        // ----------------------------------------------------
        // 5. 可中断的帧率节流
        // ----------------------------------------------------

        const auto now = std::chrono::steady_clock::now();

        if (nextFrameTime > now) {
            std::unique_lock<std::mutex> lock(m_pauseMutex);

            m_pauseCv.wait_until(
                lock,
                nextFrameTime,
                [this] {
                    return !m_threadRunning.load() ||
                           m_state.load() != State::Playing ||
                           m_seekRequested.load();
                });

            if (!m_threadRunning.load() ||
                m_state.load() != State::Playing ||
                m_seekRequested.load()) {

                av_packet_unref(m_pkt);
                continue;
            }
        }

        nextFrameTime = std::chrono::steady_clock::now()
                        + frameDuration;

        // ----------------------------------------------------
        // 6. 更新估算播放位置
        // ----------------------------------------------------

        const int64_t incrementMs =
            std::max<int64_t>(1, 1000 / std::max(1, m_fps.load()));

        int64_t position = m_positionMs.load() + incrementMs;

        const int64_t duration = m_durationMs.load();

        if (duration > 0) {
            position = std::min(position, duration);
        }

        m_positionMs = position;

        // ----------------------------------------------------
        // 7. 原始 H.264 直接推流
        // ----------------------------------------------------

        if (m_streamer && m_threadRunning.load() &&
            m_state.load() == State::Playing &&
            !m_seekRequested.load()) {

            m_streamer->PushH264(
                m_pkt->data,
                static_cast<size_t>(m_pkt->size));
        }

        av_packet_unref(m_pkt);
    }

    m_threadRunning = false;

    LOG_I << "VideoReader ReadLoop end";
}
