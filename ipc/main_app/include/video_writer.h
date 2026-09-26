#pragma once

#include <fstream>
#include <string>
#include <chrono>

class VideoWriter
{
public:
    VideoWriter();
    ~VideoWriter();

    void OpenNewFile();
    void VideoWrite(const char* data, size_t size);

    // 设置参数及配置
    void SetFileMaxSize(size_t size);

    // 获取参数及配置
    std::string FileName() const { return m_filename; }
    size_t size() const { return m_totalWritten; }
    size_t MaxSize() const { return m_maxFileSize; }

private:
    std::string GetTimestampFilename(const std::string& prefix, const std::string& suffix);

    std::ofstream m_ofs;
    std::string m_filename;
    size_t m_totalWritten;
    size_t m_maxFileSize;
};
