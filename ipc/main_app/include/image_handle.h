#pragma once

#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>

struct RawFrame {
    uint8_t *data;      // NV12 数据（Y + UV 连续）
    size_t   size;      // 总字节数
    uint32_t width;     // 图像宽度
    uint32_t height;    // 图像高度
};

namespace ImageHandle
{
cv::Mat RawFrameToBGR(const RawFrame &frame);
};