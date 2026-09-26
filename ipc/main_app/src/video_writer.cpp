#include <sstream>
#include <iomanip>
#include <sys/stat.h>
#include <sys/types.h>
#include <ctime>
#include "mainapp_log.h"
#include "file_system.h"
#include "video_writer.h"

VideoWriter::VideoWriter()
    : m_totalWritten(0)
    , m_maxFileSize(SIZE_128MB) // 每个文件默认最大 128MB
{
    m_filename = GetTimestampFilename("media", "h264");
    m_ofs = std::ofstream(m_filename, std::ios::binary);
    if (!m_ofs.is_open()) {
        LOG_E << "Failed to open: " << m_filename;
        return;
    }
}

VideoWriter::~VideoWriter()
{
    if (m_ofs.is_open()) {
        m_ofs.close();
    }
}

void VideoWriter::OpenNewFile()
{
    m_ofs.close();
    m_totalWritten = 0;

    m_filename = GetTimestampFilename("media/", "h264");
    m_ofs = std::ofstream(m_filename, std::ios::binary);
    if (!m_ofs.is_open()) {
        LOG_E << "Failed to open: " << m_filename;
        return;
    }
}

void VideoWriter::VideoWrite(const char* data, size_t size)
{
    if (!m_ofs.is_open()) {
        LOG_E << "File not open";
        return;
    }

    m_ofs.write(data, size);
    m_totalWritten += size;

    // 检查当前写入文件大小
    if (m_totalWritten >= m_maxFileSize) {
        OpenNewFile();
    }
}

void VideoWriter::SetFileMaxSize(size_t size)
{
    m_maxFileSize = size;
}

std::string VideoWriter::GetTimestampFilename(const std::string& prefix, const std::string& suffix)
{
    auto now = std::chrono::system_clock::now();
    auto timeTNow = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;

    auto tm = std::localtime(&timeTNow);

    // 日期目录
    std::stringstream dateSs;
    dateSs << std::put_time(tm, "%Y%m%d");
    std::string dateDir = dateSs.str();

    // 文件名
    std::stringstream fileSs;
    fileSs << std::put_time(tm, "%H%M%S")
           << "_" << std::setfill('0') << std::setw(3) << ms.count()
           << "." << suffix;
    std::string fileName = fileSs.str();

    // 完整目录
    std::string fullDir = prefix + "/" + dateDir;

    // 递归创建整个路径
    if (!FileSystem::MakeDirs(fullDir)) {
        LOG_E << "Failed to create dir: " << fullDir;
    }

    // 返回完整路径
    return fullDir + "/" + fileName;
}
