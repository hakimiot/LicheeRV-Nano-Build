#include <chrono>
#include <thread>
#include <atomic>
#include <csignal>
#include "mainapp_log.h"
#include "camera_capture.h"
#include "image_handle.h"
#include "decode_handle.h"

#include <opencv2/imgcodecs.hpp>

std::atomic<bool> g_running(true);

void signalHandler(int sig)
{
    (void)sig;
    g_running.store(false, std::memory_order_relaxed);
}

int main(int argc, char **argv)
{
    int fpsTime = 1000 / 30;

    CameraConfig cfg;
    CameraCapture cameraCapture(cfg);

    FILE *fp = fopen("./video.h264", "wb");
    if (fp == nullptr) {
        LOG_E << "fopen failed";
        return -1;
    }

    signal(SIGINT, signalHandler);

    std::this_thread::sleep_for(std::chrono::milliseconds(5000));

    while (g_running) {
        // 采集前
        auto t1 = std::chrono::steady_clock::now();

        VENC_STREAM_S stStream;
        if (cameraCapture.GetVencStream(stStream)) {
            // 遍历所有 pack，依次写入
            for (CVI_U32 i = 0; i < stStream.u32PackCount; i++) {
                fwrite(
                    stStream.pstPack[i].pu8Addr +
                        stStream.pstPack[i].u32Offset,
                    1,
                    stStream.pstPack[i].u32Len -
                        stStream.pstPack[i].u32Offset,
                    fp
                );
            }
            cameraCapture.ReleaseVencStream(stStream);
        }

        // 采集后
        auto t2 = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1).count();
        LOG_I << "ViGetChnFrame took " << elapsed << " ms";
    }

    // 回收资源
    LOG_I << "exit main_app...";
    fclose(fp);

    return 0;
}
