#include <sstream>
#include <iomanip>
#include <ctime>
#include <fstream>
#include "mainapp_log.h"
#include "file_system.h"
#include "video_writer.h"

VideoWriter::VideoWriter(uint32_t width, uint32_t height, double fps)
    : m_totalWritten(0)
    , m_maxFileSize(SIZE_128MB)
    , m_width(width)
    , m_height(height)
    , m_fps(fps)
{
    m_filename = GetTimestampFilename("/media", "h264");
    m_ofs = std::ofstream(m_filename, std::ios::binary);
    if (!m_ofs.is_open()) {
        LOG_E << "Failed to open: " << m_filename;
        return;
    }
    m_startTime = std::chrono::system_clock::now();
}

VideoWriter::~VideoWriter()
{
    if (m_ofs.is_open()) {
        WriteMetadataJson();
        m_ofs.close();
    }
}

void VideoWriter::OpenNewFile()
{
    // 先写当前文件的 JSON，再关文件
    if (m_ofs.is_open()) {
        WriteMetadataJson();
        m_ofs.close();
    }

    // 重置统计
    m_totalWritten  = 0;
    m_frameCount    = 0;
    m_firstPtsUs    = 0;
    m_lastPtsUs     = 0;
    m_keyFrameCount = 0;

    m_filename = GetTimestampFilename("/media", "h264");
    m_ofs = std::ofstream(m_filename, std::ios::binary);
    if (!m_ofs.is_open()) {
        LOG_E << "Failed to open: " << m_filename;
        return;
    }
    m_startTime = std::chrono::system_clock::now();
}

void VideoWriter::VideoWrite(const char* data, size_t size)
{
    VideoWrite(data, size, 0, false);
}

void VideoWriter::VideoWrite(const char* data, size_t size, uint64_t ptsUs, bool isKeyFrame)
{
    if (!m_ofs.is_open()) {
        LOG_E << "File not open";
        return;
    }

    m_ofs.write(data, size);
    m_totalWritten += size;

    if (m_frameCount == 0) {
        m_firstPtsUs = ptsUs;
    }
    m_lastPtsUs = ptsUs;
    m_frameCount++;
    if (isKeyFrame) {
        m_keyFrameCount++;
    }

    if (m_totalWritten >= m_maxFileSize) {
        OpenNewFile();
    }
}

void VideoWriter::SetFileMaxSize(size_t size)
{
    m_maxFileSize = size;
}

void VideoWriter::SetResolution(uint32_t width, uint32_t height)
{
    ApplyParamsChange(width, height, m_fps);
}

void VideoWriter::SetFps(double fps)
{
    ApplyParamsChange(m_width, m_height, fps);
}

void VideoWriter::SetVideoParams(uint32_t width, uint32_t height, double fps)
{
    ApplyParamsChange(width, height, fps);
}

void VideoWriter::ApplyParamsChange(uint32_t width, uint32_t height, double fps)
{
    if (m_width == width && m_height == height && m_fps == fps) {
        return;
    }

    if (m_ofs.is_open() && m_totalWritten > 0) {
        OpenNewFile();
    }

    // 更新为新参数
    m_width  = width;
    m_height = height;
    m_fps    = fps;

    LOG_I << "Video params changed, new file: " << m_filename
          << " (" << width << "x" << height << " @ " << fps << " fps)";
}

void VideoWriter::WriteMetadataJson()
{
    if (m_filename.empty()) return;

    std::string jsonPath = m_filename + ".json";

    // 时长优先用 PTS 差，PTS 无效时用帧数 / 帧率
    double durationSec = 0.0;
    if (m_lastPtsUs > m_firstPtsUs) {
        durationSec = (m_lastPtsUs - m_firstPtsUs) / 1e6;
    } else if (m_fps > 0.0 && m_frameCount > 0) {
        durationSec = static_cast<double>(m_frameCount) / m_fps;
    }

    auto startTimeT = std::chrono::system_clock::to_time_t(m_startTime);
    auto endTimeT   = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());

    std::stringstream ss;
    ss << "{\n";
    ss << "  \"file\": \"" << m_filename << "\",\n";
    ss << "  \"width\": " << m_width << ",\n";
    ss << "  \"height\": " << m_height << ",\n";
    ss << "  \"fps\": " << std::fixed << std::setprecision(3) << m_fps << ",\n";
    ss << "  \"frame_count\": " << m_frameCount << ",\n";
    ss << "  \"keyframe_count\": " << m_keyFrameCount << ",\n";
    ss << "  \"duration_sec\": " << std::fixed << std::setprecision(3) << durationSec << ",\n";
    ss << "  \"file_size_bytes\": " << m_totalWritten << ",\n";
    ss << "  \"bitrate_kbps\": "
       << (durationSec > 0.0 ? static_cast<uint64_t>(m_totalWritten * 8 / durationSec / 1000) : 0)
       << ",\n";
    ss << "  \"start_time\": " << startTimeT << ",\n";
    ss << "  \"end_time\": " << endTimeT << "\n";
    ss << "}\n";

    std::ofstream jf(jsonPath, std::ios::binary);
    if (!jf.is_open()) {
        LOG_E << "Failed to open json: " << jsonPath;
        return;
    }
    jf << ss.str();
    jf.close();

    LOG_I << "Metadata written: " << jsonPath
          << " frames=" << m_frameCount
          << " duration=" << durationSec << "s";
}

std::string VideoWriter::GetTimestampFilename(const std::string& prefix, const std::string& suffix)
{
    auto now = std::chrono::system_clock::now();
    auto timeTNow = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;

    auto tm = std::localtime(&timeTNow);

    std::stringstream dateSs;
    dateSs << std::put_time(tm, "%Y%m%d");
    std::string dateDir = dateSs.str();

    std::stringstream fileSs;
    fileSs << std::put_time(tm, "%H%M%S")
           << "_" << std::setfill('0') << std::setw(3) << ms.count()
           << "." << suffix;
    std::string fileName = fileSs.str();

    std::string fullDir = prefix + "/" + dateDir;

    if (!FileSystem::MakeDirs(fullDir)) {
        LOG_E << "Failed to create dir: " << fullDir;
    }

    return fullDir + "/" + fileName;
}
