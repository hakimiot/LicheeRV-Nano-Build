#include <fstream>
#include <nlohmann/json.hpp>

extern "C" {
#include <libavutil/time.h>
#include <libavutil/error.h>
}

#include "mainapp_log.h"
#include "video_reader.h"

// ============================================================
//  构造 / 析构
// ============================================================
VideoReader::VideoReader() = default;

VideoReader::~VideoReader()
{
    Close();   // Close 里已经调用 Stop
}

// ============================================================
//  打开
// ============================================================
bool VideoReader::Open(const std::string& videoPath, const std::string& jsonPath)
{
    Close();
    m_path = videoPath;

    // 1. 打开视频（只用于读包，时长不从这里取）
    int ret = avformat_open_input(&m_fmtCtx, videoPath.c_str(), nullptr, nullptr);
    if (ret < 0) {
        char err[256]; av_strerror(ret, err, sizeof(err));
        LOG_E << "avformat_open_input failed: " << err;
        return false;
    }

    ret = avformat_find_stream_info(m_fmtCtx, nullptr);
    if (ret < 0) {
        LOG_E << "avformat_find_stream_info failed";
        Close();
        return false;
    }

    // 2. 找视频流
    for (unsigned i = 0; i < m_fmtCtx->nb_streams; ++i) {
        if (m_fmtCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            m_videoIndex = i;
            break;
        }
    }
    if (m_videoIndex < 0) {
        LOG_E << "No video stream found";
        Close();
        return false;
    }

    AVStream* st = m_fmtCtx->streams[m_videoIndex];
    if (st->codecpar->codec_id != AV_CODEC_ID_H264) {
        LOG_E << "Not an H.264 stream";
        Close();
        return false;
    }
    m_codecPar = st->codecpar;

    // 3. 帧率（先用码流猜，后面如果 JSON 有 fps 会覆盖）
    AVRational fr = av_guess_frame_rate(m_fmtCtx, st, nullptr);
    if (fr.num > 0 && fr.den > 0) {
        m_fps = static_cast<int>(fr.num / fr.den + 0.5);
        if (m_fps <= 0) m_fps = 30;
    }

    m_pkt = av_packet_alloc();

    // 4. 从 json 读总时长 + fps
    std::string jsonFile = jsonPath.empty() ? (videoPath + ".json") : jsonPath;
    if (!LoadDurationFromJson(jsonFile)) {
        LOG_W << "Failed to read duration from json: " << jsonFile
              << ", duration will be 0";
        m_durationMs = 0;
    }

    m_positionMs = 0;
    m_state      = State::Idle;

    LOG_I << "VideoReader opened: " << videoPath
          << " duration(json)=" << m_durationMs << "ms"
          << " fps=" << m_fps
          << " res=" << m_codecPar->width << "x" << m_codecPar->height;

    return true;
}

// ============================================================
//  从 json 读总时长 / fps
//  支持字段：
//    duration_ms / durationMs     明确毫秒
//    duration_sec                 明确秒
//    duration                     默认毫秒，除非有 "unit":"s"
//    fps                          帧率
// ============================================================
bool VideoReader::LoadDurationFromJson(const std::string& jsonPath)
{
    std::ifstream ifs(jsonPath);
    if (!ifs.is_open()) {
        LOG_W << "Cannot open json: " << jsonPath;
        return false;
    }

    nlohmann::json j;
    try {
        ifs >> j;
    } catch (const std::exception& e) {
        LOG_W << "JSON parse error: " << e.what();
        return false;
    }

    // ---- 时长 ----
    if (j.contains("duration_ms") && j["duration_ms"].is_number()) {
        m_durationMs = j["duration_ms"].get<int64_t>();
    } else if (j.contains("durationMs") && j["durationMs"].is_number()) {
        m_durationMs = j["durationMs"].get<int64_t>();
    } else if (j.contains("duration_sec") && j["duration_sec"].is_number()) {
        double sec = j["duration_sec"].get<double>();
        m_durationMs = static_cast<int64_t>(sec * 1000.0);
    } else if (j.contains("duration") && j["duration"].is_number()) {
        double v = j["duration"].get<double>();

        bool isSeconds = false;
        if (j.contains("unit") && j["unit"].is_string()) {
            std::string u = j["unit"].get<std::string>();
            if (u == "s" || u == "sec" || u == "second" || u == "seconds") {
                isSeconds = true;
            }
        }

        m_durationMs = isSeconds
                     ? static_cast<int64_t>(v * 1000.0)
                     : static_cast<int64_t>(v);
    } else {
        LOG_W << "No duration field in json: " << jsonPath;
        return false;
    }

    if (m_durationMs < 0) m_durationMs = 0;

    // ---- fps（优先用 JSON 的，覆盖码流猜的） ----
    if (j.contains("fps") && j["fps"].is_number()) {
        int fpsFromJson = static_cast<int>(j["fps"].get<double>() + 0.5);
        if (fpsFromJson > 0) {
            m_fps = fpsFromJson;
        }
    }

    LOG_I << "Duration loaded from json: " << m_durationMs << "ms"
          << " fps=" << m_fps;
    return true;
}

// ============================================================
//  关闭
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
    m_codecPar   = nullptr;
    m_videoIndex = -1;
    m_durationMs = 0;
    m_positionMs = 0;
    m_fps        = 30;
}

// ============================================================
//  播放 / 暂停 / 停止
// ============================================================
bool VideoReader::Play()
{
    if (!m_fmtCtx) return false;

    State s = m_state.load();
    if (s == State::Playing) return true;

    if (s == State::Paused) {
        {
            std::lock_guard<std::mutex> lock(m_pauseMutex);
            m_state = State::Playing;
        }
        m_pauseCv.notify_all();
        return true;
    }

    if (m_threadRunning) return true;

    // 如果上一次的线程还没回收，先 join
    if (m_readThread.joinable()) {
        m_readThread.join();
    }

    m_threadRunning = true;
    m_state = State::Playing;
    m_readThread = std::thread(&VideoReader::ReadLoop, this);
    return true;
}

void VideoReader::Pause()
{
    if (m_state.load() != State::Playing) return;

    {
        std::lock_guard<std::mutex> lock(m_pauseMutex);
        m_state = State::Paused;
    }
    LOG_I << "Paused at " << m_positionMs.load() << "ms";
}

void VideoReader::Stop()
{
    // 重要：不能因为状态是 Idle 就跳过 join
    {
        std::lock_guard<std::mutex> lock(m_pauseMutex);
        m_state = State::Stopped;
    }
    m_pauseCv.notify_all();

    m_threadRunning = false;

    if (m_readThread.joinable()) {
        m_readThread.join();
    }

    if (m_streamer) {
        m_streamer->ClearQueue();
    }

    if (m_fmtCtx) {
        std::lock_guard<std::mutex> lock(m_seekMutex);
        av_seek_frame(m_fmtCtx, m_videoIndex, 0, AVSEEK_FLAG_BACKWARD);
    }
    m_positionMs = 0;
    m_state      = State::Idle;

    LOG_I << "Stopped";
}

// ============================================================
//  Seek
// ============================================================
bool VideoReader::Seek(int64_t ms)
{
    if (!m_fmtCtx) return false;

    if (ms < 0) ms = 0;
    if (m_durationMs > 0 && ms > m_durationMs) ms = m_durationMs;

    if (m_threadRunning) {
        m_seekTargetMs  = ms;
        m_seekRequested = true;
        m_pauseCv.notify_all();
        return true;
    }

    return SeekInternal(ms);
}

std::string VideoReader::GetStateString()
{
    State state = m_state.load();
    switch (state) {
    case State::Idle: return "idel";
    case State::Playing: return "playing";
    case State::Paused: return "paused";
    case State::Stopped: return "stopped";
    default: return "unknown";
    }
}

bool VideoReader::SeekInternal(int64_t ms)
{
    std::lock_guard<std::mutex> lock(m_seekMutex);

    AVStream* st = m_fmtCtx->streams[m_videoIndex];
    int64_t ts = av_rescale_q(ms, {1, 1000}, st->time_base);

    int ret = av_seek_frame(m_fmtCtx, m_videoIndex, ts, AVSEEK_FLAG_BACKWARD);
    if (ret < 0) {
        char err[256]; av_strerror(ret, err, sizeof(err));
        LOG_E << "av_seek_frame failed: " << err;
        return false;
    }

    if (m_streamer) {
        m_streamer->ClearQueue();
        m_streamer->ResetPts(0);
    }

    m_positionMs = ms;

    LOG_I << "Seek to " << ms << "ms (keyframe aligned)";
    return true;
}

// ============================================================
//  读帧线程
// ============================================================
void VideoReader::ReadLoop()
{
    if (m_streamer) {
        static uint64_t packetCount = 0;
        ++packetCount;

        if (packetCount % 30 == 0) {
            LOG_I << "[REPLAY] packetCount=" << packetCount
                << ", size=" << m_pkt->size
                << ", pts=" << m_pkt->pts
                << ", dts=" << m_pkt->dts
                << ", key=" << ((m_pkt->flags & AV_PKT_FLAG_KEY) != 0);
        }

        m_streamer->PushH264(m_pkt->data, m_pkt->size);
    } else {
        LOG_E << "[REPLAY] streamer is null";
    }

    LOG_I << "VideoReader ReadLoop start";

    AVStream*  st = m_fmtCtx->streams[m_videoIndex];
    AVRational tb = st->time_base;

    auto frameDuration = std::chrono::microseconds(
        1000000 / (m_fps > 0 ? m_fps : 30));
    auto nextFrameTime = std::chrono::steady_clock::now();

    int64_t basePts    = AV_NOPTS_VALUE;
    int64_t seekBaseMs = 0;

    while (m_threadRunning) {

        // ---------- 1. 暂停 ----------
        {
            std::unique_lock<std::mutex> lock(m_pauseMutex);
            m_pauseCv.wait_for(lock, std::chrono::milliseconds(50), [this] {
                return m_state.load() != State::Paused
                    || m_seekRequested.load()
                    || !m_threadRunning.load();
            });

            if (!m_threadRunning) break;
            if (m_state.load() == State::Stopped) break;

            if (m_state.load() == State::Paused && !m_seekRequested.load()) {
                continue;
            }
        }

        // ---------- 2. Seek ----------
        if (m_seekRequested.exchange(false)) {
            int64_t target = m_seekTargetMs.load();
            if (SeekInternal(target)) {
                basePts       = AV_NOPTS_VALUE;
                seekBaseMs    = target;
                nextFrameTime = std::chrono::steady_clock::now();
            }
            if (m_state.load() == State::Paused) continue;
        }

        // ---------- 3. 读包 ----------
        int ret;
        {
            std::lock_guard<std::mutex> lock(m_seekMutex);
            ret = av_read_frame(m_fmtCtx, m_pkt);
        }

        if (ret < 0) {
            LOG_I << "End of file";
            if (m_streamer) m_streamer->WaitUntilQueueEmpty(3000);
            m_state         = State::Idle;
            m_threadRunning = false;
            break;
        }

        if (m_pkt->stream_index != m_videoIndex) {
            av_packet_unref(m_pkt);
            continue;
        }

        // ---------- 4. 节流（无论有没有 PTS 都节流） ----------
        bool hasPts = (m_pkt->pts != AV_NOPTS_VALUE);

        if (hasPts) {
            if (basePts == AV_NOPTS_VALUE) basePts = m_pkt->pts;

            int64_t relPts = m_pkt->pts - basePts;
            int64_t relMs  = av_rescale_q(relPts, tb, {1, 1000});
            m_positionMs   = seekBaseMs + relMs;
        } else {
            // 没有 PTS，按帧率估算位置
            m_positionMs += (1000 / (m_fps > 0 ? m_fps : 30));
        }

        // 无论有没有 PTS，都按帧率节流
        {
            auto now = std::chrono::steady_clock::now();
            if (nextFrameTime > now) {
                std::unique_lock<std::mutex> lock(m_pauseMutex);
                m_pauseCv.wait_until(lock, nextFrameTime, [this] {
                    return m_state.load() != State::Playing
                        || m_seekRequested.load();
                });
                if (m_state.load() != State::Playing || m_seekRequested.load()) {
                    av_packet_unref(m_pkt);
                    continue;
                }
            }
            nextFrameTime += frameDuration;
        }

        // ---------- 5. 推流 ----------
        if (m_streamer) {
            m_streamer->PushH264(m_pkt->data, m_pkt->size);
        }

        av_packet_unref(m_pkt);
    }

    LOG_I << "VideoReader ReadLoop end";
}
