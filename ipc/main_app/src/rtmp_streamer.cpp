#include <chrono>
#include <cstring>
extern "C" {
#include <libavutil/opt.h>
#include <libavutil/error.h>
}
#include "mainapp_log.h"
#include "rtmp_streamer.h"

RTMPStreamer::RTMPStreamer()
{
    av_log_set_level(AV_LOG_WARNING);
}

RTMPStreamer::~RTMPStreamer()
{
    Stop();
}

bool RTMPStreamer::Init(const std::string& url, int width, int height, int fps)
{
    m_url = url;
    m_width = width;
    m_height = height;
    m_fps = fps;

    LOG_I << "RTMP init: " << url << " (" << width << "x" << height << "@" << fps << "fps)";

    if (!InitInternal()) {
        LOG_W << "RTMP init failed, will retry in background";
        m_connected = false;
        m_reconnectRequested = true;
    } else {
        m_connected = true;
    }
    return true;
}

bool RTMPStreamer::InitInternal()
{
    Cleanup();

    // 1. 创建输出上下文
    int ret = avformat_alloc_output_context2(&m_formatCtx, nullptr, "flv", m_url.c_str());
    if (!m_formatCtx) {
        LOG_E << "Failed to create output context";
        return false;
    }

    // 2. 创建视频流（H.264 直通，不经过编码器）
    m_stream = avformat_new_stream(m_formatCtx, nullptr);
    if (!m_stream) {
        LOG_E << "Failed to create video stream";
        Cleanup();
        return false;
    }

    m_stream->codecpar->codec_type = AVMEDIA_TYPE_VIDEO;
    m_stream->codecpar->codec_id   = AV_CODEC_ID_H264;
    m_stream->codecpar->width      = m_width;
    m_stream->codecpar->height     = m_height;
    // 输入流的时间基用 1/fps，输出时再转换
    m_stream->time_base            = {1, m_fps};

    // 3. 分配 packet
    m_packet = av_packet_alloc();
    if (!m_packet) {
        LOG_E << "Failed to allocate packet";
        Cleanup();
        return false;
    }

    // 4. 打开 RTMP 连接
    if (!(m_formatCtx->oformat->flags & AVFMT_NOFILE)) {
        ret = avio_open(&m_formatCtx->pb, m_url.c_str(), AVIO_FLAG_WRITE);
        if (ret < 0) {
            char errbuf[256];
            av_strerror(ret, errbuf, sizeof(errbuf));
            LOG_E << "Failed to connect RTMP server: " << errbuf;
            Cleanup();
            return false;
        }
    }

    // 5. 这里不写 header，等收到 SPS/PPS 后再写
    m_pts = 0;
    m_extradataCopied = false;
    m_headerWritten = false;
    LOG_I << "RTMP streamer initialized (header pending SPS/PPS)";
    return true;
}

void RTMPStreamer::Start()
{
    if (m_running) {
        return;
    }

    m_running = true;
    m_streamThread = std::thread(&RTMPStreamer::StreamLoop, this);
    LOG_I << "Stream thread started";
}

void RTMPStreamer::Stop()
{
    if (!m_running) {
        return;
    }

    m_running = false;
    m_queueCv.notify_all();
    if (m_streamThread.joinable()) {
        m_streamThread.join();
    }

    if (m_formatCtx && m_connected && m_headerWritten) {
        av_write_trailer(m_formatCtx);
    }

    Cleanup();
    m_connected = false;
    LOG_I << "Streamer stopped";
}

void RTMPStreamer::PushH264(const uint8_t* data, size_t len)
{
    if (!m_running || len == 0) {
        return;
    }

    std::lock_guard<std::mutex> lock(m_queueMutex);
    if (m_queue.size() >= m_maxQueueSize) {
        m_queue.pop();
    }

    H264Packet pkt;
    pkt.data.assign(data, data + len);
    m_queue.push(std::move(pkt));
    m_queueCv.notify_one();
}

void RTMPStreamer::StreamLoop()
{
    LOG_I << "Stream loop start";

    while (m_running) {
        // 重连逻辑：失败越多，间隔越长（5s, 10s, 20s, 40s, 最多 60s）
        if (m_reconnectRequested || !m_connected) {
            auto now = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
                now - m_lastReconnectTime).count();

            int interval = 5 * (1 << std::min(m_reconnectCount, 3));
            if (interval > 60) interval = 60;

            if (elapsed >= interval) {
                m_lastReconnectTime = now;
                if (ReinitRtmp()) {
                    LOG_I << "RTMP reconnected";
                    m_reconnectCount = 0;
                } else {
                    m_reconnectCount++;
                    LOG_W << "RTMP reconnect failed, count=" << m_reconnectCount;
                }
            }
        }

        H264Packet pkt;
        {
            std::unique_lock<std::mutex> lock(m_queueMutex);
            if (m_queue.empty()) {
                m_queueCv.wait_for(lock, std::chrono::milliseconds(10));
                continue;
            }
            pkt = std::move(m_queue.front());
            m_queue.pop();
        }

        if (!m_connected || !m_formatCtx) continue;

        bool isKeyFrame = IsKeyFrame(pkt.data.data(), pkt.data.size());

        // 提取 SPS/PPS（跨 packet 累积，直到两个都找到）
        if (!m_extradataCopied) {
            if (!ExtractSpsPps(pkt.data.data(), pkt.data.size())) {
                continue;   // 还没凑齐，跳过这一帧
            }
        }

        // extradata 就绪后，写 FLV header
        if (m_extradataCopied && !m_headerWritten) {
            int ret = avformat_write_header(m_formatCtx, nullptr);
            if (ret < 0) {
                char errbuf[256];
                av_strerror(ret, errbuf, sizeof(errbuf));
                LOG_E << "Failed to write header: " << errbuf;
                m_connected = false;
                m_reconnectRequested = true;
                continue;
            }
            m_headerWritten = true;
            avio_flush(m_formatCtx->pb);   // 确保 header 立即发出
            LOG_I << "FLV header written with SPS/PPS";
        }

        if (!m_headerWritten) continue;

        // 用 av_new_packet 分配 packet，确保 buf 被正确设置
        av_packet_unref(m_packet);
        int ret = av_new_packet(m_packet, pkt.data.size());
        if (ret < 0) {
            LOG_E << "av_new_packet failed";
            continue;
        }
        memcpy(m_packet->data, pkt.data.data(), pkt.data.size());

        // 关键：时间戳只用 m_pts++，让 av_packet_rescale_ts 统一转换
        m_packet->stream_index = m_stream->index;
        m_packet->pts = m_pts++;
        m_packet->dts = m_packet->pts;
        m_packet->duration = 1;
        m_packet->flags = isKeyFrame ? AV_PKT_FLAG_KEY : 0;

        av_packet_rescale_ts(m_packet, {1, m_fps}, m_stream->time_base);

        ret = av_interleaved_write_frame(m_formatCtx, m_packet);
        if (ret < 0) {
            char errbuf[256];
            av_strerror(ret, errbuf, sizeof(errbuf));
            LOG_W << "Write frame failed: " << errbuf;
            m_connected = false;
            m_reconnectRequested = true;
        } else {
            avio_flush(m_formatCtx->pb);
        }
    }

    LOG_I << "Stream loop end";
}

bool RTMPStreamer::ReinitRtmp()
{
    LOG_I << "Reinit RTMP connection...";
    bool result = InitInternal();
    m_connected = result;
    if (result) {
        m_reconnectRequested = false;
        LOG_I << "RTMP reconnect success";
    }
    return result;
}

void RTMPStreamer::Cleanup()
{
    if (m_packet) {
        av_packet_free(&m_packet);
    }
    if (m_formatCtx) {
        if (m_formatCtx->pb) {
            avio_closep(&m_formatCtx->pb);
        }
        avformat_free_context(m_formatCtx);
        m_formatCtx = nullptr;
    }
    m_stream = nullptr;

    // 释放解析器
    if (m_parser) {
        av_parser_close(m_parser);
        m_parser = nullptr;
    }
    if (m_parserCtx) {
        avcodec_free_context(&m_parserCtx);
        m_parserCtx = nullptr;
    }

    m_headerWritten = false;
    m_extradataCopied = false;
    m_cachedSps.clear();
    m_cachedPps.clear();
}

void RTMPStreamer::SetRtmpUrl(const std::string& url)
{
    std::lock_guard<std::mutex> lock(m_queueMutex);
    m_url = url;
    m_reconnectRequested = true;
    m_connected = false;
}

void RTMPStreamer::SetResolution(int width, int height)
{
    if (width <= 0 || height <= 0) {
        return;
    }

    if (m_width == width && m_height == height) {
        return;
    }

    m_width = width;
    m_height = height;
    m_reconnectRequested = true;
    m_connected = false;
}

void RTMPStreamer::SetFps(int fps)
{
    if (fps <= 0 || m_fps == fps) {
        return;
    }

    m_fps = fps;
    m_reconnectRequested = true;
    m_connected = false;
}

// 提取 SPS/PPS
bool RTMPStreamer::ExtractSpsPps(const uint8_t* data, size_t len)
{
    std::vector<uint8_t> sps;
    std::vector<uint8_t> pps;

    size_t pos = 0;

    while (pos < len) {
        size_t startCodeSize = 0;

        if (!FindStartCode(data, len, pos, startCodeSize)) {
            break;
        }

        size_t nalStart = pos + startCodeSize;
        size_t nextStart = nalStart;
        size_t nextStartCodeSize = 0;
        if (FindStartCode(data, len, nalStart, nextStartCodeSize)) {

            // 从 nalStart 开始搜索，
            for (size_t i = nalStart; i + 3 < len; ++i) {
                bool is3 = data[i] == 0 && data[i + 1] == 0 && data[i + 2] == 1;
                bool is4 = i + 4 < len && data[i] == 0 && data[i + 1] == 0 && data[i + 2] == 0 && data[i + 3] == 1;

                if (is3 || is4) {
                    nextStart = i;
                    break;
                }
            }
        } else {
            nextStart = len;
        }

        if (nalStart >= nextStart) {
            pos = nextStart;
            continue;
        }

        uint8_t nalType = data[nalStart] & 0x1F;

        if (nalType == 7) {
            sps.assign(data + nalStart, data + nextStart);
        } else if (nalType == 8) {
            pps.assign(data + nalStart, data + nextStart);
        }

        if (!sps.empty() && !pps.empty()) {
            break;
        }

        pos = nextStart;
    }

    if (sps.empty() || pps.empty()) {
        return false;
    }

    std::vector<uint8_t> extradata;

    // Annex-B SPS
    extradata.push_back(0x00);
    extradata.push_back(0x00);
    extradata.push_back(0x00);
    extradata.push_back(0x01);
    extradata.insert(extradata.end(), sps.begin(), sps.end());

    // Annex-B PPS
    extradata.push_back(0x00);
    extradata.push_back(0x00);
    extradata.push_back(0x00);
    extradata.push_back(0x01);
    extradata.insert(extradata.end(), pps.begin(), pps.end());

    uint8_t* extra = static_cast<uint8_t*>(av_malloc(extradata.size() + AV_INPUT_BUFFER_PADDING_SIZE));

    if (!extra) {
        return false;
    }

    memcpy(extra, extradata.data(), extradata.size());
    memset(extra + extradata.size(), 0, AV_INPUT_BUFFER_PADDING_SIZE);

    av_freep(&m_stream->codecpar->extradata);

    m_stream->codecpar->extradata = extra;
    m_stream->codecpar->extradata_size = static_cast<int>(extradata.size());
    m_extradataCopied = true;

    LOG_I << "H264 SPS/PPS extracted, " << "SPS=" << sps.size() << " PPS=" << pps.size();

    return true;
}

bool RTMPStreamer::IsKeyFrame(const uint8_t* data, size_t len)
{
    for (size_t i = 0; i + 3 < len; ++i) {
        size_t nalOffset = 0;

        if (data[i] == 0 && data[i + 1] == 0 && data[i + 2] == 1) {
            nalOffset = i + 3;
        } else if (i + 4 < len && data[i] == 0 && data[i + 1] == 0 && data[i + 2] == 0 && data[i + 3] == 1) {
            nalOffset = i + 4;
        } else {
            continue;
        }

        if (nalOffset < len) {
            uint8_t nalType = data[nalOffset] & 0x1F;
            if (nalType == 5) {
                return true;
            }
        }
    }

    return false;
}

bool RTMPStreamer::FindStartCode(const uint8_t* data, size_t size, size_t pos, size_t& startCodeSize)
{
    for (size_t i = pos; i + 3 < size; ++i) {

        if (data[i] == 0 && data[i + 1] == 0 && data[i + 2] == 1) {
            startCodeSize = 3;
            return true;
        }

        if (i + 4 < size && data[i] == 0 && data[i + 1] == 0 && data[i + 2] == 0 && data[i + 3] == 1) {
            startCodeSize = 4;
            return true;
        }
    }

    return false;
}
