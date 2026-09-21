#pragma once

#include "sample_comm.h"

class CameraCapture
{
public:
    CameraCapture();
    ~CameraCapture();

    CVI_S32 SensorDumpYuv();

private:
    int SysViInit(void);
    void SysViDeinit();
    
    CVI_S32 ViGetChnFrame(CVI_U8 chn);
    long DiffInUs(struct timespec t1, struct timespec t2);

    SAMPLE_VI_CONFIG_S m_stViConfig;
    SAMPLE_INI_CFG_S m_stIniCfg;

    bool m_initStatus;
};
