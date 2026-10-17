#pragma once

#include <fstream>
#include <string>
#include <chrono>
#include <cstdint>

class VideoWriter
{
public:
    VideoWriter(uint32_t width, uint32_t height, double fps);
    ~VideoWriter();

    void OpenNewFile();
    void VideoWrite(const char* data, size_t size);
    void VideoWrite(const char* data, size_t size, uint64_t ptsUs, bool isKeyFrame = false);

    void SetFileMaxSize(size_t size);

    // 单独设置分辨率
    void SetResolution(uint32_t width, uint32_t height);

    // 单独设置帧率
    void SetFps(double fps);

    // 一键设置分辨率 + 帧率
    void SetVideoParams(uint32_t width, uint32_t height, double fps);

    // 查询
    std::string FileName() const { return m_filename; }
    size_t size() const { return m_totalWritten; }
    size_t MaxSize() const { return m_maxFileSize; }

    uint32_t Width()  const { return m_width; }
    uint32_t Height() const { return m_height; }
    double   Fps()    const { return m_fps; }

private:
    std::string GetTimestampFilename(const std::string& prefix, const std::string& suffix);
    void WriteMetadataJson();

    // 切换新文件
    void ApplyParamsChange(uint32_t width, uint32_t height, double fps);

    std::ofstream m_ofs;
    std::string m_filename;
    size_t m_totalWritten;
    size_t m_maxFileSize;

    uint32_t m_width;
    uint32_t m_height;
    double   m_fps;

    uint64_t m_frameCount    = 0;
    uint64_t m_firstPtsUs    = 0;
    uint64_t m_lastPtsUs     = 0;
    uint64_t m_keyFrameCount = 0;

    std::chrono::system_clock::time_point m_startTime;
};
