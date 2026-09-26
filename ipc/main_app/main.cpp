#include <chrono>
#include <thread>
#include <atomic>
#include <csignal>
#include "mainapp_log.h"
#include "camera_capture.h"
#include "ring_queue.h"
#include "video_writer.h"
#include "rtmp_streamer.h"

std::atomic<bool> g_running(true);

static void SignalHandler(int sig)
{
    (void)sig;
    g_running.store(false, std::memory_order_relaxed);
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

        // 一个 VENC_STREAM 对应一个编码帧
        StreamPacket packet;

        for (CVI_U32 i = 0; i < stStream.u32PackCount; ++i) {
            VENC_PACK_S& pack = stStream.pstPack[i];

            if (!pack.pu8Addr || pack.u32Len <= pack.u32Offset) {
                continue;
            }

            const uint8_t* data = pack.pu8Addr + pack.u32Offset;
            const size_t len = pack.u32Len - pack.u32Offset;

            packet.data.insert(packet.data.end(), data, data + len);
        }

        cameraCapture.ReleaseVencStream(stStream);

        if (!packet.data.empty()) {
            // 一个 VENC_STREAM 只 Push 一次
            streamer.PushH264(packet.data.data(), packet.data.size());
            queue.push(std::move(packet));
        }

        auto t2 = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1).count();

        if (elapsed > 5) {
            LOG_I << "Capture took " << elapsed << " ms";
        }
    }
}

/**
 * 写文件线程
 */
static void SaveThread(RingQueue<StreamPacket>& queue, std::atomic<bool>& running)
{
    VideoWriter writer;
    StreamPacket packet;

    while (running && queue.pop(packet)) {
        writer.VideoWrite(reinterpret_cast<const char*>(packet.data.data()), packet.data.size());
    }
}

/**
 * 主函数
 */
int main(int argc, char **argv)
{
    int fpsTime = 1000 / 30;

    CameraConfig cfg;
    CameraCapture cameraCapture(cfg);

    RTMPStreamer streamer;
    streamer.Init("rtmp://192.168.43.4:1935/live/stream", cfg.width, cfg.height, cfg.dstFps);
    streamer.Start();

    RingQueue<StreamPacket> queue(30);

    signal(SIGINT, SignalHandler);

    std::this_thread::sleep_for(std::chrono::milliseconds(5000));

    // 创建线程
    std::thread capThread(CaptureThread, std::ref(cameraCapture), std::ref(queue), std::ref(g_running), std::ref(streamer));
    std::thread savThread(SaveThread, std::ref(queue), std::ref(g_running));

    while (g_running) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    // 回收资源
    LOG_I << "exit main_app...";
    queue.stop();
    capThread.join();
    savThread.join();

    streamer.Stop();

    return 0;
}
