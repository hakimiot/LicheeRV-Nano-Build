#include "image_handle.h"

cv::Mat ImageHandle::RawFrameToBGR(const RawFrame &frame)
{
    if (frame.data == nullptr || frame.width == 0 || frame.height == 0) {
        return cv::Mat();
    }

    // 把 NV12 数据包装成 cv::Mat
    cv::Mat yuvNv12(frame.height * 3 / 2, frame.width, CV_8UC1,
                     (void *)frame.data);

    // NV12 -> BGR
    cv::Mat bgr;
    cv::cvtColor(yuvNv12, bgr, cv::COLOR_YUV2BGR_NV21);

    return bgr;
}
