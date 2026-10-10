#pragma once

// cvi
#include "cvi_venc.h"
#include "linux/cvi_comm_venc.h"
#include "sample_comm.h"

struct RawFrame {
    uint8_t *data;      // NV12 数据（Y + UV 连续）
    size_t   size;      // 总字节数
    uint32_t width;     // 图像宽度
    uint32_t height;    // 图像高度
};

// 配置结构体
struct CameraConfig {
    uint32_t width     = 1280;
    uint32_t height    = 720;
    uint32_t srcFps    = 30;
    uint32_t dstFps    = 30;
    uint32_t bitrate   = 4096;
    uint32_t gop       = 30;
    uint32_t maxWidth  = 2560;
    uint32_t maxHeight = 1440;
};

class CameraCapture
{
public:
    explicit CameraCapture(const CameraConfig &cfg = CameraConfig());
    ~CameraCapture();

    RawFrame ViGetChnFrame(CVI_U8 chn);
    bool GetVencStream(VENC_STREAM_S &stStream);
    void ReleaseVencStream(VENC_STREAM_S &stStream);
    
    int SysViInit();
    void SysViDeinit();

    // 获取参数及配置
    CameraConfig config() const { return m_cfg; };

private:
    long DiffInUs(struct timespec t1, struct timespec t2);

    SAMPLE_VI_CONFIG_S m_stViConfig;
    SAMPLE_INI_CFG_S m_stIniCfg;

    VENC_CHN m_vencChn = 0;

    bool m_initStatus;

    uint8_t *m_frameBuffer;
    size_t   m_bufferSize;

    CameraConfig m_cfg;
};
