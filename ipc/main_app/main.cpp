#include <chrono>
#include <thread>
#include <atomic>
#include <csignal>
#include <SimpleIni.h>

#include "mainapp_log.h"
#include "camera_capture.h"
#include "ring_queue.h"
#include "video_writer.h"
#include "video_reader.h"
#include "rtmp_streamer.h"
#include "http_server.h"

std::atomic<bool> g_running(true);

static void SignalHandler(int sig)
{
    (void)sig;
    g_running.store(false, std::memory_order_relaxed);
}

static void SignalRegister(void)
{
    signal(SIGINT, SignalHandler);
    signal(SIGTERM, SignalHandler);
    signal(SIGQUIT, SignalHandler);
    signal(SIGHUP, SignalHandler);
}

static bool IsKeyFrame(const uint8_t* data, size_t len)
{
    size_t i = 0;
    while (i + 3 < len) {
        size_t startCodeSize = 0;
        if (i + 4 < len &&
            data[i] == 0 && data[i+1] == 0 &&
            data[i+2] == 0 && data[i+3] == 1) {
            startCodeSize = 4;
        } else if (data[i] == 0 && data[i+1] == 0 && data[i+2] == 1) {
            startCodeSize = 3;
        } else {
            ++i;
            continue;
        }

        size_t nalStart = i + startCodeSize;
        if (nalStart >= len) break;

        uint8_t nalType = data[nalStart] & 0x1F;
        if (nalType == 5) return true;   // IDR

        i = nalStart;
    }
    return false;
}

/**
 * 采集和实时推流线程
 */
static void CaptureThread(
    CameraCapture& cameraCapture,
    RingQueue<StreamPacket>& queue,
    std::atomic<bool>& running,
    RTMPStreamer& streamer)
{
    while (running.load(std::memory_order_relaxed)) {
        auto t1 = std::chrono::steady_clock::now();
        VENC_STREAM_S stStream{};

        if (!cameraCapture.GetVencStream(stStream)) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            continue;
        }

        StreamPacket packet;
        uint64_t framePtsUs      = 0;
        bool     frameIsKeyFrame = false;

        for (CVI_U32 i = 0; i < stStream.u32PackCount; ++i) {
            VENC_PACK_S& pack = stStream.pstPack[i];

            if (!pack.pu8Addr || pack.u32Len <= pack.u32Offset) {
                continue;
            }

            const uint8_t* data = pack.pu8Addr + pack.u32Offset;
            const size_t   len  = pack.u32Len - pack.u32Offset;

            packet.data.insert(packet.data.end(), data, data + len);

            if (i == 0) {
                framePtsUs = pack.u64PTS;
                LOG_D << "PTS=" << pack.u64PTS << " type=" << (int)pack.DataType.enH264EType;
            }

            if (pack.DataType.enH264EType == H264E_NALU_IDRSLICE) {
                frameIsKeyFrame = true;
            }
        }

        cameraCapture.ReleaseVencStream(stStream);

        if (!packet.data.empty()) {
            packet.pts        = framePtsUs;
            packet.isKeyFrame = frameIsKeyFrame;

            streamer.PushH264(packet.data.data(), packet.data.size());
            queue.push(std::move(packet));
        }

        auto t2 = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1).count();

        if (elapsed > 5) {
            LOG_D << "Capture took " << elapsed << " ms";
        }
    }
}

/**
 * 写文件线程
 */
static void SaveThread(RingQueue<StreamPacket>& queue,
    std::atomic<bool>& running,
    CameraConfig cfg)
{
    VideoWriter writer(cfg.width, cfg.height, cfg.srcFps);
    StreamPacket packet;

    while (running && queue.pop(packet)) {
        writer.VideoWrite(
            reinterpret_cast<const char*>(packet.data.data()),
            packet.data.size(),
            packet.pts,
            packet.isKeyFrame
        );
    }
}

/**
 * HTTP线程
 */
static void HttpThread(HttpServer& server, DataContext& context)
{
    server.start();
}

/**
 * 主函数
 */
int main(int argc, char **argv)
{
    CSimpleIniA ini;
    CameraConfig cfg;
    
    // 读取 ini 配置文件
    SI_Error rc = ini.LoadFile("/etc/main_app/config.ini");
    if (rc < 0) {
        LOG_E << "Failed to load config";
        return -1;
    }

    cfg.width = ini.GetLongValue("capture", "width", 1280);
    cfg.height = ini.GetLongValue("capture", "height", 720);
    cfg.srcFps = ini.GetLongValue("capture", "srcFps", 30);
    std::string rtmpUrl = ini.GetValue("rtmp", "url", "");

    // 初始化摄像头采集器
    CameraCapture cameraCapture(cfg);

    SignalRegister();

    // 初始化直播推流器
    RTMPStreamer liveStreamer;
    liveStreamer.Init(rtmpUrl + "/live/stream/" + "123456", cfg.width, cfg.height, cfg.dstFps);

    // 初始化回放推流器
    RTMPStreamer replayStreamer;
    replayStreamer.Init(rtmpUrl + "/replay/stream/" + "123456", cfg.width, cfg.height, cfg.dstFps);

    VideoReader videoReader;
    videoReader.SetStreamer(&replayStreamer);

    RingQueue<StreamPacket> queue(30);

    std::this_thread::sleep_for(std::chrono::milliseconds(5000));

    // 初始化数据上下文
    DataContext context = {
        g_running,
        cameraCapture,
        liveStreamer,
        replayStreamer,
        videoReader
    };
    HttpServer server(context);

    // 创建线程
    std::thread capThread(CaptureThread, std::ref(cameraCapture), std::ref(queue), std::ref(g_running), std::ref(liveStreamer));
    std::thread savThread(SaveThread, std::ref(queue), std::ref(g_running), std::ref(cfg));
    std::thread httpThread(HttpThread, std::ref(server), std::ref(context));

    while (g_running) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    // 回收资源
    LOG_I << "exit main_app...";
    queue.stop();

    LOG_I << "exit http server";
    server.stop();

    LOG_I << "wait thread capture";
    capThread.join();

    LOG_I << "wait thread save";
    savThread.join();

    LOG_I << "wait thread http";
    httpThread.join();

    LOG_I << "stop live streamer";
    liveStreamer.Stop();

    LOG_I << "stop replay streamer";
    replayStreamer.Stop();

    LOG_I << "stop video reader";
    videoReader.Stop();

    return 0;
}
