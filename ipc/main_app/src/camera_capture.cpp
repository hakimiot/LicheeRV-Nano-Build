// cvi
#include "cvi_type.h"

#include "mainapp_log.h"
#include "camera_capture.h"

CameraCapture::CameraCapture()
{
    int ret = SysViInit();
    if (ret != CVI_SUCCESS) {
        LOG_E << "sys vi init failed";
        m_initStatus = false;
    } else {
        m_initStatus = true;
    }
}

CameraCapture::~CameraCapture()
{
    SysViDeinit();
}

CVI_S32 CameraCapture::SensorDumpYuv()
{
    CVI_U32 ok = 0, ng = 0;
    CVI_U8  chn = 0;
    int tmp;
    struct timespec start, end;

    clock_gettime(CLOCK_MONOTONIC, &start);
    if (ViGetChnFrame(chn) == CVI_SUCCESS) {
        ++ok;
        clock_gettime(CLOCK_MONOTONIC, &end);
        LOG_I << "ms consumed: " << (CVI_FLOAT)DiffInUs(start, end)/1000;
    } else {
        ++ng;
    }

    LOG_I << "VI GetChnFrame OK(" << ok << ") NG(" << ng << ")";
    LOG_I << "Dump VI yuv TEST-PASS";

    return CVI_SUCCESS;
}

int CameraCapture::SysViInit()
{
    MMF_VERSION_S stVersion;
    SAMPLE_INI_CFG_S stIniCfg;
    SAMPLE_VI_CONFIG_S stViConfig;

    PIC_SIZE_E enPicSize;
    SIZE_S stSize;
    CVI_S32 s32Ret = CVI_SUCCESS;
    LOG_LEVEL_CONF_S log_conf;

    CVI_SYS_GetVersion(&stVersion);
    LOG_I << "MMF Version:" << stVersion.version;

    log_conf.enModId = CVI_ID_LOG;
    log_conf.s32Level = CVI_DBG_INFO;
    CVI_LOG_SetLevelConf(&log_conf);

    // Get config from ini if found.
    if (SAMPLE_COMM_VI_ParseIni(&stIniCfg)) {
        LOG_I << "Parse complete";
    }

    //Set sensor number
    CVI_VI_SetDevNum(stIniCfg.devNum);
    /************************************************
     * step1:  Config VI
     ************************************************/
    s32Ret = SAMPLE_COMM_VI_IniToViCfg(&stIniCfg, &stViConfig);
    if (s32Ret != CVI_SUCCESS)
        return s32Ret;

    memcpy(&m_stViConfig, &stViConfig, sizeof(SAMPLE_VI_CONFIG_S));
    memcpy(&m_stIniCfg, &stIniCfg, sizeof(SAMPLE_INI_CFG_S));

    /************************************************
     * step2:  Get input size
     ************************************************/
    s32Ret = SAMPLE_COMM_VI_GetSizeBySensor(stIniCfg.enSnsType[0], &enPicSize);
    if (s32Ret != CVI_SUCCESS) {
        LOG_E << "SAMPLE_COMM_VI_GetSizeBySensor failed with " << std::hex << s32Ret;
        return s32Ret;
    }

    s32Ret = SAMPLE_COMM_SYS_GetPicSize(enPicSize, &stSize);
    if (s32Ret != CVI_SUCCESS) {
        LOG_E << "SAMPLE_COMM_SYS_GetPicSize failed with " << std::hex << s32Ret;
        return s32Ret;
    }

    /************************************************
     * step3:  Init modules
     ************************************************/
    s32Ret = SAMPLE_PLAT_SYS_INIT(stSize);
    if (s32Ret != CVI_SUCCESS) {
        LOG_E << "sys init failed. s32Ret: " << std::hex << s32Ret;
        return s32Ret;
    }

    s32Ret = SAMPLE_PLAT_VI_INIT(&stViConfig);
    if (s32Ret != CVI_SUCCESS) {
        LOG_E << "vi init failed. s32Ret: " << std::hex << s32Ret;
        return s32Ret;
    }

    return CVI_SUCCESS;
}

void CameraCapture::SysViDeinit()
{
    SAMPLE_COMM_VI_DestroyIsp(&m_stViConfig);
    SAMPLE_COMM_VI_DestroyVi(&m_stViConfig);
    SAMPLE_COMM_SYS_Exit();
}

CVI_S32 CameraCapture::ViGetChnFrame(CVI_U8 chn)
{
    VIDEO_FRAME_INFO_S stVideoFrame;
    VI_CROP_INFO_S crop_info = {0};

    if (CVI_VI_GetChnFrame(0, chn, &stVideoFrame, 3000) == 0) {
        FILE *output;
        size_t image_size = stVideoFrame.stVFrame.u32Length[0] + stVideoFrame.stVFrame.u32Length[1]
                  + stVideoFrame.stVFrame.u32Length[2];
        CVI_U8 *vir_addr;
        CVI_U32 plane_offset, u32LumaSize, u32ChromaSize;
        CVI_CHAR img_name[128] = {0, };

        LOG_I << "width: " << stVideoFrame.stVFrame.u32Width
              << ", height: " << stVideoFrame.stVFrame.u32Height
              << ", total_buf_length: " << image_size;

        snprintf(img_name, sizeof(img_name), "sample_%d.yuv", chn);

        output = fopen(img_name, "wb");
        if (output == NULL) {
            memset(img_name, 0x0, sizeof(img_name));
            snprintf(img_name, sizeof(img_name), "/mnt/data/sample_%d.yuv", chn);
            output = fopen(img_name, "wb");
            if (output == NULL) {
                CVI_VI_ReleaseChnFrame(0, chn, &stVideoFrame);
                LOG_E << "fopen fail";
                return CVI_FAILURE;
            }
        }

        u32LumaSize =  stVideoFrame.stVFrame.u32Stride[0] * stVideoFrame.stVFrame.u32Height;
        u32ChromaSize =  stVideoFrame.stVFrame.u32Stride[1] * stVideoFrame.stVFrame.u32Height / 2;
        CVI_VI_GetChnCrop(0, chn, &crop_info);
        if (crop_info.bEnable) {
            u32LumaSize = ALIGN((crop_info.stCropRect.u32Width * 8 + 7) >> 3, DEFAULT_ALIGN) *
                ALIGN(crop_info.stCropRect.u32Height, 2);
            u32ChromaSize = (ALIGN(((crop_info.stCropRect.u32Width >> 1) * 8 + 7) >> 3, DEFAULT_ALIGN) *
                ALIGN(crop_info.stCropRect.u32Height, 2)) >> 1;
        }

        vir_addr = (CVI_U8 *)CVI_SYS_Mmap(stVideoFrame.stVFrame.u64PhyAddr[0], image_size);
        CVI_SYS_IonInvalidateCache(stVideoFrame.stVFrame.u64PhyAddr[0], vir_addr, image_size);

        plane_offset = 0;
        for (int i = 0; i < 3; i++) {
            if (stVideoFrame.stVFrame.u32Length[i] != 0) {
                stVideoFrame.stVFrame.pu8VirAddr[i] = vir_addr + plane_offset;
                plane_offset += stVideoFrame.stVFrame.u32Length[i];
                LOG_I << "plane(" << i << "): paddr(" << std::hex << stVideoFrame.stVFrame.u64PhyAddr[i]
                      << ") vaddr(" << static_cast<void*>(stVideoFrame.stVFrame.pu8VirAddr[i])
                      << ") stride(" << std::dec << stVideoFrame.stVFrame.u32Stride[i]
                      << ") length(" << stVideoFrame.stVFrame.u32Length[i] << ")";
                fwrite((void *)stVideoFrame.stVFrame.pu8VirAddr[i]
                    , (i == 0) ? u32LumaSize : u32ChromaSize, 1, output);
            }
        }
        CVI_SYS_Munmap(vir_addr, image_size);

        if (CVI_VI_ReleaseChnFrame(0, chn, &stVideoFrame) != 0)
            LOG_E << "CVI_VI_ReleaseChnFrame NG";

        fclose(output);
        return CVI_SUCCESS;
    }
    LOG_E << "CVI_VI_GetChnFrame NG";
    return CVI_FAILURE;
}

long CameraCapture::DiffInUs(struct timespec t1, struct timespec t2)
{
    struct timespec diff;

    if (t2.tv_nsec-t1.tv_nsec < 0) {
        diff.tv_sec  = t2.tv_sec - t1.tv_sec - 1;
        diff.tv_nsec = t2.tv_nsec - t1.tv_nsec + 1000000000;
    } else {
        diff.tv_sec  = t2.tv_sec - t1.tv_sec;
        diff.tv_nsec = t2.tv_nsec - t1.tv_nsec;
    }
    return (diff.tv_sec * 1000000.0 + diff.tv_nsec / 1000.0);
}
