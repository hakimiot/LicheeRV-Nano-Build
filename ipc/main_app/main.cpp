#include <chrono>
#include <thread>
#include "mainapp_log.h"
#include "camera_capture.h"

/*
// OpenCV
#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>

// FFmpeg
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <libswscale/swscale.h>
}

// Paho MQTT C++
#include <mqtt/client.h>

int libtest(int argc, char **argv)
{
    std::cout << "===== 库链接测试 =====" << std::endl;

    // 1. OpenCV
    std::cout << "\n[1] OpenCV" << std::endl;
    std::cout << "  OpenCV version: " << CV_VERSION << std::endl;
    cv::Mat img(100, 100, CV_8UC3, cv::Scalar(0, 255, 0));
    cv::Mat gray;
    cv::cvtColor(img, gray, cv::COLOR_BGR2GRAY);
    std::cout << "  Mat created: " << img.cols << "x" << img.rows
              << ", gray channels: " << gray.channels() << std::endl;

    // 2. FFmpeg
    std::cout << "\n[2] FFmpeg" << std::endl;
    std::cout << "  avcodec version: " << avcodec_version() << std::endl;
    std::cout << "  avformat version: " << avformat_version() << std::endl;
    std::cout << "  avutil version: " << avutil_version() << std::endl;
    const AVCodec *codec = avcodec_find_encoder(AV_CODEC_ID_H264);
    if (codec) {
        std::cout << "  H264 encoder found: " << codec->name << std::endl;
    } else {
        std::cout << "  H264 encoder not found (maybe disabled)" << std::endl;
    }

    // 3. Paho MQTT C++
    std::cout << "\n[3] Paho MQTT C++" << std::endl;
    const std::string broker = "tcp://localhost:1883";
    const std::string client_id = "rv_nano_test";
    try {
        mqtt::client client(broker, client_id);
        std::cout << "  MQTT client created (broker: " << broker << ")" << std::endl;
        std::cout << "  Client ID: " << client_id << std::endl;
        // 不实际连接，只验证对象能创建
    } catch (const mqtt::exception &e) {
        std::cout << "  MQTT exception: " << e.what() << std::endl;
    }

    std::cout << "\n===== 测试完成 =====" << std::endl;
    return 0;
}
*/

int main(int argc, char **argv) 
{
    // libtest(argc, argv);

    CameraCapture cameraCapture;
    
    std::this_thread::sleep_for(std::chrono::milliseconds(5000));

    cameraCapture.SensorDumpYuv();
    return 0;
}
