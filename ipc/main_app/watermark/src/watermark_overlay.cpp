
#include "watermark_overlay.h"
#include "mainapp_log.h"

#include <ctime>
#include <cstdio>
#include <cstring>
#include <vector>
#include <string>
#include <cstdint>

namespace {

constexpr uint16_t kTransparent = 0x0000;
constexpr uint16_t kOpaqueBlack = 0x8000;
constexpr uint16_t kOpaqueWhite = 0xFFFF;

// 5x7 ASCII 点阵字库。
// 每个字形由 7 行组成，每行低 5 位对应 5 个像素。
// 1 表示字符笔画，0 表示背景。
struct Glyph {
    char ch;
    uint8_t rows[7];
};

const Glyph kGlyphs[] = {
    {' ', {0, 0, 0, 0, 0, 0, 0}},
    {'-', {0, 0, 0, 31, 0, 0, 0}},
    {':', {0, 12, 12, 0, 12, 12, 0}},
    {'.', {0, 0, 0, 0, 0, 12, 12}},
    {'/', {1, 2, 2, 4, 8, 8, 16}},
    {'I', {31, 4, 4, 4, 4, 4, 31}},
    {'D', {30, 17, 17, 17, 17, 17, 30}},
    {'0', {14, 17, 19, 21, 25, 17, 14}},
    {'1', {4, 12, 4, 4, 4, 4, 14}},
    {'2', {14, 17, 1, 2, 4, 8, 31}},
    {'3', {30, 1, 1, 14, 1, 1, 30}},
    {'4', {2, 6, 10, 18, 31, 2, 2}},
    {'5', {31, 16, 16, 30, 1, 1, 30}},
    {'6', {14, 16, 16, 30, 17, 17, 14}},
    {'7', {31, 1, 2, 4, 8, 8, 8}},
    {'8', {14, 17, 17, 14, 17, 17, 14}},
    {'9', {14, 17, 17, 15, 1, 1, 14}}
};

} // namespace

WatermarkOverlay::WatermarkOverlay() = default;

WatermarkOverlay::~WatermarkOverlay()
{
    Deinit();
}

bool WatermarkOverlay::Init(
    const std::string& deviceId,
    int vpssGrp,
    int vpssChn)
{
    if (m_created) {
        return true;
    }

    m_deviceId = deviceId.empty() ? "UNKNOWN" : deviceId;
    m_vpssGrp = vpssGrp;
    m_vpssChn = vpssChn;

    RGN_ATTR_S attr{};
    attr.enType = OVERLAY_RGN;
    attr.unAttr.stOverlay.enPixelFormat = PIXEL_FORMAT_ARGB_1555;
    attr.unAttr.stOverlay.stSize.u32Width = kRegionWidth;
    attr.unAttr.stOverlay.stSize.u32Height = kRegionHeight;
    attr.unAttr.stOverlay.u32BgColor = kTransparent;
    attr.unAttr.stOverlay.u32CanvasNum = 1;

    CVI_S32 ret = CVI_RGN_Create(kHandle, &attr);
    if (ret != CVI_SUCCESS) {
        LOG_E << "CVI_RGN_Create failed: " << ret;
        return false;
    }

    m_created = true;

    // 先写入初始位图。
    if (!Update()) {
        LOG_E << "Initial watermark update failed";
        Deinit();
        return false;
    }

    MMF_CHN_S channel{};
    channel.enModId = CVI_ID_VPSS;
    channel.s32DevId = m_vpssGrp;
    channel.s32ChnId = m_vpssChn;

    RGN_CHN_ATTR_S channelAttr{};
    channelAttr.bShow = CVI_TRUE;
    channelAttr.enType = OVERLAY_RGN;
    channelAttr.unChnAttr.stOverlayChn.stPoint.s32X = 16;
    channelAttr.unChnAttr.stOverlayChn.stPoint.s32Y = 16;
    channelAttr.unChnAttr.stOverlayChn.u32Layer = 0;

    ret = CVI_RGN_AttachToChn(kHandle, &channel, &channelAttr);
    if (ret != CVI_SUCCESS) {
        LOG_E << "CVI_RGN_AttachToChn failed: " << ret;
        Deinit();
        return false;
    }

    m_attached = true;

    LOG_I << "Watermark attached: ID=" << m_deviceId
          << ", VPSS=" << m_vpssGrp
          << "/" << m_vpssChn;

    return true;
}

bool WatermarkOverlay::Update()
{
    if (!m_created) {
        LOG_E << "Watermark Region is not created";
        return false;
    }

    const std::time_t now = std::time(nullptr);

    std::tm localTime{};
    if (localtime_r(&now, &localTime) == nullptr) {
        LOG_E << "localtime_r failed";
        return false;
    }

    char timeText[32]{};

    if (std::strftime(
            timeText,
            sizeof(timeText),
            "%Y-%m-%d %H:%M:%S",
            &localTime) == 0) {
        LOG_E << "strftime failed";
        return false;
    }

    const std::string text =
        "ID: " + m_deviceId + "  " + timeText;

    std::vector<uint16_t> pixels;

    if (!CreateBitmap(text, pixels)) {
        LOG_E << "CreateBitmap failed";
        return false;
    }

    BITMAP_S bitmap{};
    bitmap.enPixelFormat = PIXEL_FORMAT_ARGB_1555;
    bitmap.u32Width = kBitmapWidth;
    bitmap.u32Height = kBitmapHeight;
    bitmap.pData = pixels.data();

    LOG_I << "SetBitMap:"
      << " handle=" << kHandle
      << ", pixelFormat=" << bitmap.enPixelFormat
      << ", width=" << bitmap.u32Width
      << ", height=" << bitmap.u32Height
      << ", data=" << bitmap.pData;

    const size_t expectedSize =
    static_cast<size_t>(bitmap.u32Width) *
    bitmap.u32Height *
    sizeof(uint16_t);

    if (pixels.size() * sizeof(uint16_t) != expectedSize) {
        LOG_E << "Invalid bitmap buffer size";
        return false;
    }

    const CVI_S32 ret = CVI_RGN_SetBitMap(kHandle, &bitmap);

    if (ret != CVI_SUCCESS) {
        LOG_E << "CVI_RGN_SetBitMap failed: " << ret;
        return false;
    }

    LOG_D << "Watermark updated: " << text;

    return true;
}

bool WatermarkOverlay::GetGlyph(char ch, uint8_t rows[7])
{
    for (const Glyph& glyph : kGlyphs) {
        if (glyph.ch == ch) {
            std::memcpy(rows, glyph.rows, 7);
            return true;
        }
    }

    // 未支持的字符显示为空格。
    std::memset(rows, 0, 7);
    return false;
}

void WatermarkOverlay::DrawText(
    std::vector<uint16_t>& pixels,
    const std::string& text,
    int x,
    int y,
    int scale)
{
    if (scale <= 0) {
        return;
    }

    int cursorX = x;

    for (char ch : text) {
        uint8_t rows[7]{};
        GetGlyph(ch, rows);

        // 字符间距 1 个点阵像素。
        for (int row = 0; row < 7; ++row) {
            for (int col = 0; col < 5; ++col) {
                if ((rows[row] & (1U << (4 - col))) == 0) {
                    continue;
                }

                for (int dy = 0; dy < scale; ++dy) {
                    for (int dx = 0; dx < scale; ++dx) {
                        const int px =
                            cursorX + col * scale + dx;

                        const int py =
                            y + row * scale + dy;

                        if (px < 0 || py < 0 ||
                            px >= static_cast<int>(kWidth) ||
                            py >= static_cast<int>(kHeight)) {
                            continue;
                        }

                        // 白色字形，周围绘制黑色描边。
                        for (int oy = -1; oy <= 1; ++oy) {
                            for (int ox = -1; ox <= 1; ++ox) {
                                if (ox == 0 && oy == 0) {
                                    continue;
                                }

                                const int bx = px + ox;
                                const int by = py + oy;

                                if (bx >= 0 && by >= 0 &&
                                    bx < static_cast<int>(kWidth) &&
                                    by < static_cast<int>(kHeight)) {
                                    pixels[
                                        static_cast<size_t>(by) *
                                        kWidth + bx
                                    ] = kOpaqueBlack;
                                }
                            }
                        }

                        pixels[
                            static_cast<size_t>(py) *
                            kWidth + px
                        ] = kOpaqueWhite;
                    }
                }
            }
        }

        cursorX += 6 * scale;

        if (cursorX >= static_cast<int>(kWidth)) {
            break;
        }
    }
}

bool WatermarkOverlay::CreateBitmap(
    const std::string& text,
    std::vector<uint16_t>& pixels)
{
    const size_t pixelCount =
        static_cast<size_t>(kWidth) * kHeight;

    pixels.assign(pixelCount, kTransparent);

    // scale=4 时每个字符约 20x28 像素。
    DrawText(pixels, text, 8, 12, 4);

    return true;
}

void WatermarkOverlay::Deinit()
{
    if (!m_created) {
        return;
    }

    MMF_CHN_S channel{};
    channel.enModId = CVI_ID_VPSS;
    channel.s32DevId = m_vpssGrp;
    channel.s32ChnId = m_vpssChn;

    if (m_attached) {
        const CVI_S32 ret =
            CVI_RGN_DetachFromChn(kHandle, &channel);

        if (ret != CVI_SUCCESS) {
            LOG_E << "CVI_RGN_DetachFromChn failed: " << ret;
        } else {
            m_attached = false;
        }
    }

    const CVI_S32 ret = CVI_RGN_Destroy(kHandle);

    if (ret != CVI_SUCCESS) {
        LOG_E << "CVI_RGN_Destroy failed: " << ret;
        return;
    }

    m_created = false;
    m_attached = false;

    LOG_I << "Watermark deinitialized";
}
