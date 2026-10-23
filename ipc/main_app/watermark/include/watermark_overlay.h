
#pragma once

#include <string>
#include <vector>
#include <cstdint>

#include "linux/cvi_comm_region.h"
#include "cvi_region.h"

class WatermarkOverlay {
public:
    WatermarkOverlay();
    ~WatermarkOverlay();

    WatermarkOverlay(const WatermarkOverlay&) = delete;
    WatermarkOverlay& operator=(const WatermarkOverlay&) = delete;

    bool Init(
        const std::string& deviceId,
        int vpssGrp = 0,
        int vpssChn = 0);

    // 更新设备编号和时间的位图，不重新创建 Region。
    bool Update();

    // 解除绑定并销毁 Region。
    void Deinit();

private:
    bool CreateBitmap(
        const std::string& text,
        std::vector<uint16_t>& pixels);

    static bool GetGlyph(
        char ch,
        uint8_t rows[7]);

    static void DrawText(
        std::vector<uint16_t>& pixels,
        const std::string& text,
        int x,
        int y,
        int scale);

private:
    // Region 画布尺寸
    static constexpr CVI_U32 kRegionWidth = 928;
    static constexpr CVI_U32 kRegionHeight = 96;

    // 实际位图尺寸，必须小于 Region 尺寸
    static constexpr CVI_U32 kBitmapWidth = 896;
    static constexpr CVI_U32 kBitmapHeight = 64;

    static constexpr CVI_U32 kWidth = 896;
    static constexpr CVI_U32 kHeight = 64;
    static constexpr RGN_HANDLE kHandle = 10;

    std::string m_deviceId;
    int m_vpssGrp = 0;
    int m_vpssChn = 0;

    bool m_created = false;
    bool m_attached = false;
};
