// cvi
#include "cvi_type.h"

#include "mainapp_log.h"
#include "camera_capture.h"

CameraCapture::CameraCapture(const CameraConfig &cfg)
    : m_cfg(cfg)
    , m_frameBuffer(nullptr)
    , m_bufferSize(0)
    , m_initStatus(false)
{
    int ret = SysViInit();
    if (ret != CVI_SUCCESS) {
        LOG_E << "sys vi init failed";
    }
}

CameraCapture::~CameraCapture()
{
    SysViDeinit();

    if (m_frameBuffer) {
        free(m_frameBuffer);
        m_frameBuffer = nullptr;
    }
}

RawFrame CameraCapture::ViGetChnFrame(CVI_U8 chn)
{
    RawFrame result = {nullptr, 0, 0, 0};
    VIDEO_FRAME_INFO_S stVideoFrame;

    if (m_initStatus == false) {
        LOG_E << "System not init";
        return result;
    }

    // 从 VPSS 通道取帧
    if (CVI_VPSS_GetChnFrame(0, chn, &stVideoFrame, 3000) != 0) {
        LOG_E << "CVI_VPSS_GetChnFrame NG";
        return result;
    }

    size_t image_size = stVideoFrame.stVFrame.u32Length[0]
                      + stVideoFrame.stVFrame.u32Length[1]
                      + stVideoFrame.stVFrame.u32Length[2];
    CVI_U8 *vir_addr;
    CVI_U32 plane_offset;
    CVI_U32 width  = stVideoFrame.stVFrame.u32Width;
    CVI_U32 height = stVideoFrame.stVFrame.u32Height;

    // 映射物理地址到虚拟地址
    vir_addr = (CVI_U8 *)CVI_SYS_Mmap(stVideoFrame.stVFrame.u64PhyAddr[0], image_size);
    CVI_SYS_IonInvalidateCache(stVideoFrame.stVFrame.u64PhyAddr[0], vir_addr, image_size);

    plane_offset = 0;
    for (int i = 0; i < 3; i++) {
        if (stVideoFrame.stVFrame.u32Length[i] != 0) {
            stVideoFrame.stVFrame.pu8VirAddr[i] = vir_addr + plane_offset;
            plane_offset += stVideoFrame.stVFrame.u32Length[i];
        }
    }

    // 计算 NV21 缓冲区大小（Y + UV）
    size_t nv21_size = (size_t)width * height * 3 / 2;

    // 只在尺寸变化或首次调用时分配
    if (m_frameBuffer == nullptr || m_bufferSize < nv21_size) {
        if (m_frameBuffer) {
            free(m_frameBuffer);
        }
        m_frameBuffer = (uint8_t *)malloc(nv21_size);
        if (m_frameBuffer == nullptr) {
            LOG_E << "malloc failed";
            CVI_SYS_Munmap(vir_addr, image_size);
            CVI_VPSS_ReleaseChnFrame(0, chn, &stVideoFrame);
            return result;
        }
        m_bufferSize = nv21_size;
        LOG_I << "Frame buffer allocated: " << nv21_size << " bytes";
    }

    // 逐行拷贝 Y plane（跳过 stride padding）
    for (CVI_U32 row = 0; row < height; row++) {
        memcpy(m_frameBuffer + row * width,
               stVideoFrame.stVFrame.pu8VirAddr[0] + row * stVideoFrame.stVFrame.u32Stride[0],
               width);
    }
    // 逐行拷贝 UV plane
    for (CVI_U32 row = 0; row < height / 2; row++) {
        memcpy(m_frameBuffer + (size_t)width * height + row * width,
               stVideoFrame.stVFrame.pu8VirAddr[1] + row * stVideoFrame.stVFrame.u32Stride[1],
               width);
    }

    result.data   = m_frameBuffer;
    result.size   = nv21_size;
    result.width  = width;
    result.height = height;

    CVI_SYS_Munmap(vir_addr, image_size);

    if (CVI_VPSS_ReleaseChnFrame(0, chn, &stVideoFrame) != 0)
        LOG_E << "CVI_VPSS_ReleaseChnFrame NG";

    return result;
}

bool CameraCapture::GetVencStream(VENC_STREAM_S &stStream)
{
    if (m_initStatus == false) {
        LOG_E << "System not init";
        return false;
    }

    memset(&stStream, 0, sizeof(stStream));

    VENC_CHN_STATUS_S stStat;
    memset(&stStat, 0, sizeof(stStat));

    CVI_S32 ret = CVI_VENC_QueryStatus(m_vencChn, &stStat);

    if (ret != CVI_SUCCESS) {
        LOG_E << "CVI_VENC_QueryStatus failed, ret=0x"
              << std::hex << ret;
        return false;
    }

    LOG_D << "VENC status: "
          << "CurPacks=" << std::dec << stStat.u32CurPacks;

    if (stStat.u32CurPacks == 0) {
        return false;
    }

    stStream.pstPack =
        (VENC_PACK_S *)calloc(
            stStat.u32CurPacks,
            sizeof(VENC_PACK_S)
        );

    if (stStream.pstPack == nullptr) {
        LOG_E << "malloc VENC_PACK_S failed";
        return false;
    }

    auto t1 = std::chrono::steady_clock::now();

    ret = CVI_VENC_GetStream(
        m_vencChn,
        &stStream,
        100
    );

    auto t2 = std::chrono::steady_clock::now();

    auto elapsed =
        std::chrono::duration_cast<std::chrono::milliseconds>(
            t2 - t1
        ).count();

    if (ret != CVI_SUCCESS) {
        LOG_E << "CVI_VENC_GetStream failed, "
              << "ret=0x" << std::hex << ret
              << ", elapsed=" << std::dec << elapsed
              << " ms"
              << ", expected packs=" << stStat.u32CurPacks;

        free(stStream.pstPack);
        stStream.pstPack = nullptr;

        return false;
    }

    LOG_D << "CVI_VENC_GetStream success, "
          << "packCount=" << stStream.u32PackCount
          << ", elapsed=" << elapsed << " ms";

    return true;
}

void CameraCapture::ReleaseVencStream(VENC_STREAM_S &stStream)
{
    if (stStream.pstPack) {
        CVI_S32 s32Ret = CVI_VENC_ReleaseStream(m_vencChn, &stStream);
        if (s32Ret != CVI_SUCCESS) {
            LOG_E << "CVI_VENC_ReleaseStream NG, ret: " << s32Ret;
        }
        free(stStream.pstPack);
        stStream.pstPack = nullptr;
    }
}

int CameraCapture::SysViInit()
{
    if (m_initStatus == true) { // 重复初始化视为初始化成功，打印日志警告
        LOG_W << "System not init";
        return CVI_SUCCESS;
    }

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

    // 解析 ini 配置
    if (SAMPLE_COMM_VI_ParseIni(&stIniCfg)) {
        LOG_I << "Parse complete";
    }

    // 设置 sensor 数量
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
     * step3:  Init SYS and VI
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

    /************************************************
     * step4:  Init VPSS
     ************************************************/
    VPSS_GRP VpssGrp = 0;
    VPSS_GRP_ATTR_S stVpssGrpAttr;
    VPSS_CHN_ATTR_S astVpssChnAttr[VPSS_MAX_PHY_CHN_NUM] = {0};
    CVI_BOOL abChnEnable[VPSS_MAX_PHY_CHN_NUM] = {0};

    memset(&stVpssGrpAttr, 0, sizeof(stVpssGrpAttr));
    stVpssGrpAttr.stFrameRate.s32SrcFrameRate = -1;
    stVpssGrpAttr.stFrameRate.s32DstFrameRate = -1;
    stVpssGrpAttr.enPixelFormat = PIXEL_FORMAT_NV21;
    stVpssGrpAttr.u32MaxW = m_cfg.maxWidth;
    stVpssGrpAttr.u32MaxH = m_cfg.maxHeight;
    stVpssGrpAttr.u8VpssDev = 0;

    // 使用配置变量
    astVpssChnAttr[0].u32Width  = m_cfg.width;
    astVpssChnAttr[0].u32Height = m_cfg.height;
    astVpssChnAttr[0].enVideoFormat = VIDEO_FORMAT_LINEAR;
    astVpssChnAttr[0].enPixelFormat = PIXEL_FORMAT_NV21;
    astVpssChnAttr[0].stFrameRate.s32SrcFrameRate = m_cfg.srcFps;
    astVpssChnAttr[0].stFrameRate.s32DstFrameRate = m_cfg.dstFps;
    astVpssChnAttr[0].u32Depth = 3;
    astVpssChnAttr[0].stAspectRatio.enMode = ASPECT_RATIO_NONE;

    abChnEnable[0] = CVI_TRUE;
    s32Ret = SAMPLE_COMM_VPSS_Init(VpssGrp, abChnEnable, &stVpssGrpAttr, astVpssChnAttr);
    if (s32Ret != CVI_SUCCESS) {
        LOG_E << "VPSS init failed";
        return s32Ret;
    }

    s32Ret = SAMPLE_COMM_VPSS_Start(VpssGrp, abChnEnable, &stVpssGrpAttr, astVpssChnAttr);
    if (s32Ret != CVI_SUCCESS) {
        LOG_E << "VPSS start failed";
        return s32Ret;
    }

    s32Ret = SAMPLE_COMM_VI_Bind_VPSS(0, 0, VpssGrp);
    if (s32Ret != CVI_SUCCESS) {
        LOG_E << "VI bind VPSS failed";
        return s32Ret;
    }

    /************************************************
     * step5:  Init VENC
     ************************************************/
    VENC_CHN_ATTR_S stVencChnAttr;
    memset(&stVencChnAttr, 0, sizeof(stVencChnAttr));

    stVencChnAttr.stVencAttr.enType = PT_H264;
    stVencChnAttr.stVencAttr.u32MaxPicWidth  = m_cfg.width;
    stVencChnAttr.stVencAttr.u32MaxPicHeight = m_cfg.height;
    stVencChnAttr.stVencAttr.u32PicWidth     = m_cfg.width;
    stVencChnAttr.stVencAttr.u32PicHeight    = m_cfg.height;
    stVencChnAttr.stVencAttr.u32BufSize      = m_cfg.width * m_cfg.height * 3 / 2;
    stVencChnAttr.stVencAttr.u32Profile = 0;
    stVencChnAttr.stVencAttr.bByFrame = CVI_TRUE;
    stVencChnAttr.stVencAttr.bSingleCore = CVI_TRUE;
    stVencChnAttr.stVencAttr.bEsBufQueueEn = CVI_TRUE;
    stVencChnAttr.stVencAttr.bIsoSendFrmEn = CVI_TRUE;
    stVencChnAttr.stVencAttr.stAttrH264e.bRcnRefShareBuf = CVI_FALSE;
    stVencChnAttr.stVencAttr.stAttrH264e.bSingleLumaBuf = CVI_FALSE;

    stVencChnAttr.stRcAttr.enRcMode = VENC_RC_MODE_H264CBR;
    stVencChnAttr.stRcAttr.stH264Cbr.u32BitRate        = m_cfg.bitrate;
    stVencChnAttr.stRcAttr.stH264Cbr.u32SrcFrameRate   = m_cfg.srcFps;
    stVencChnAttr.stRcAttr.stH264Cbr.fr32DstFrameRate  = m_cfg.dstFps;
    stVencChnAttr.stRcAttr.stH264Cbr.u32Gop            = m_cfg.gop;

    stVencChnAttr.stGopAttr.enGopMode = VENC_GOPMODE_NORMALP;

    s32Ret = CVI_VENC_CreateChn(m_vencChn, &stVencChnAttr);
    if (s32Ret != CVI_SUCCESS) {
        LOG_E << "CVI_VENC_CreateChn failed";
        return s32Ret;
    }

    // 先绑定，再启动
    s32Ret = SAMPLE_COMM_VPSS_Bind_VENC(VpssGrp, 0, m_vencChn);
    if (s32Ret != CVI_SUCCESS) {
        LOG_E << "VPSS bind VENC failed";
        return s32Ret;
    }

    VENC_RECV_PIC_PARAM_S stRecvParam;
    memset(&stRecvParam, 0, sizeof(stRecvParam));
    stRecvParam.s32RecvPicNum = -1;
    s32Ret = CVI_VENC_StartRecvFrame(m_vencChn, &stRecvParam);
    if (s32Ret != CVI_SUCCESS) {
        LOG_E << "CVI_VENC_StartRecvFrame failed";
        return s32Ret;
    }

    m_initStatus = true;
    return CVI_SUCCESS;
}

void CameraCapture::SysViDeinit()
{
    CVI_S32 ret;
    
    if (m_initStatus == false) { // 未初始化，打印日志警告
        LOG_W << "System not init";
        return;
    }

    LOG_I << "========== CameraCapture Deinit ==========";

    // ============================================================
    // 1. VENC
    // ============================================================
    // 解除 VPSS -> VENC
    ret = SAMPLE_COMM_VPSS_UnBind_VENC(0, 0, m_vencChn);
    LOG_I << "UnBind_VENC ret: " << ret;

    // 停止 VENC 接收帧
    ret = CVI_VENC_StopRecvFrame(m_vencChn);
    LOG_I << "StopRecvFrame ret: " << ret;

    // 销毁 VENC channel
    ret = CVI_VENC_DestroyChn(m_vencChn);
    LOG_I << "DestroyChn ret: " << ret;

    // ============================================================
    // 2. VPSS
    // ============================================================
    // 解除 VI -> VPSS
    ret = SAMPLE_COMM_VI_UnBind_VPSS(0, 0, 0);
    LOG_I << "UnBind_VPSS ret: " << ret;

    CVI_BOOL abChnEnable[VPSS_MAX_PHY_CHN_NUM] = {0};
    abChnEnable[0] = CVI_TRUE;

    ret = SAMPLE_COMM_VPSS_Stop(0, abChnEnable);
    LOG_I << "VPSS_Stop ret: " << ret;

    // ============================================================
    // 3. VI / ISP
    // ============================================================
    ret = SAMPLE_COMM_VI_DestroyIsp(&m_stViConfig);
    LOG_I << "VI_DestroyIsp ret: " << ret;

    ret = SAMPLE_COMM_VI_DestroyVi(&m_stViConfig);
    LOG_I << "VI_DestroyVi ret: " << ret;

    // ============================================================
    // 4. SYS / VB
    // ============================================================
    SAMPLE_COMM_SYS_Exit();
    m_initStatus = false;

    LOG_I << "========== CameraCapture Deinit Done ==========";
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
