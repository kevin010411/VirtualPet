#include "common/AppProfile.h"
#if ENABLE_CAPTURE_GAME
#include "game/CaptureScene.h"
#include "display/FrameDecoder.h"
#include "display/NumberGlyphs.h"
#include "display/Renderer.h"

namespace
{
constexpr int captureTimerEdge(int pixel) { return (pixel * 8 + 2) / 5; }

void drawCaptureTimer(uint16_t *pixels, int top, uint16_t seconds, int timerX, int timerY)
{
    // Keep the 4×6 glyphs and colors in sync with the Web CaptureTimer.
    constexpr uint32_t digitGlyphs[] = {0x699996, 0x262227, 0x69124f, 0xe1611e, 0x26af22,
                                        0xf8e196, 0x68e996, 0xf12444, 0x696996, 0x699716};
    constexpr uint32_t hourglass = 0xf9669f;
    constexpr int glyphWidth = 4;
    constexpr int glyphHeight = 6;
    constexpr int glyphAdvance = glyphWidth + 1;
    constexpr int nativeHeight = glyphHeight + 2;
    static_assert(captureTimerEdge(nativeHeight) == CaptureScene::timerHeight, "Capture timer height mismatch.");

    const bool urgent = seconds <= 5;
    uint8_t digits[3];
    uint8_t count = 0;
    do
    {
        digits[count++] = seconds % 10;
        seconds /= 10;
    } while (seconds || count < 2);
    const int nativeWidth = glyphAdvance * (count + 1) + 1;
    const int firstRow = top > timerY ? top : timerY;
    const int bandEnd = top + CaptureScene::bandRows;
    const int timerEnd = timerY + CaptureScene::timerHeight;
    const int endRow = bandEnd < timerEnd ? bandEnd : timerEnd;
    for (int y = firstRow; y < endRow; ++y)
    {
        uint16_t *row = pixels + (y - top) * CaptureAssets::screenWidth + timerX;
        // Invert the rounded pixel edges to find the source row for this screen row.
        const int sourceY = ((y - timerY) * 5 + 2) / 8;
        const int corner = sourceY == 0 || sourceY == nativeHeight - 1 ? 1 : 0;
        const uint16_t fill = sourceY == 0 ? 0x2aec : 0x1148;
        for (int x = captureTimerEdge(corner); x < captureTimerEdge(nativeWidth - corner); ++x)
            row[x] = fill;
        if (corner) continue;
        for (uint8_t index = 0; index <= count; ++index)
        {
            const uint32_t glyph = index == 0 ? hourglass : digitGlyphs[digits[count - index]];
            const uint16_t color = urgent ? 0xff12 : (index == 0 ? 0x7f5a : 0xffff);
            for (int x = 0; x < glyphWidth; ++x)
            {
                const int bit = glyphWidth * (glyphHeight + 1 - sourceY) - x - 1;
                if ((glyph & (1UL << bit)) == 0) continue;
                const int left = 1 + glyphAdvance * index + x;
                for (int px = captureTimerEdge(left); px < captureTimerEdge(left + 1); ++px)
                    row[px] = color;
            }
        }
    }
}

// Fusion Pixel 12 px zh-TW rows for 本局得分 and 中鍵返回, matching the Web preview.
constexpr uint16_t captureScoreTitle[4][11] = {
    {0x0400, 0x0400, 0xffe0, 0x1500, 0x1500, 0x1500, 0x2480, 0x2480, 0x5f40, 0x8420, 0x0400},
    {0x7fc0, 0x4040, 0x7fc0, 0x4000, 0x7fe0, 0x4020, 0x5f20, 0x5120, 0x5120, 0x5f20, 0x80c0},
    {0x2fc0, 0x4840, 0x8fc0, 0x2840, 0x4fc0, 0xc000, 0x5fe0, 0x4080, 0x5fe0, 0x4880, 0x4580},
    {0x1f00, 0x0100, 0x2080, 0x2080, 0x4040, 0xbfa0, 0x0880, 0x0880, 0x1080, 0x2080, 0xc300},
};
constexpr uint16_t captureScoreReturn[4][11] = {
    {0x0400, 0x0400, 0xffe0, 0x8420, 0x8420, 0x8420, 0xffe0, 0x0400, 0x0400, 0x0400, 0x0400},
    {0x4080, 0xbbe0, 0x08a0, 0xefe0, 0x50a0, 0xfbe0, 0x4880, 0xebe0, 0xd880, 0x6c80, 0x93e0},
    {0x8fe0, 0x4800, 0x0800, 0xcfe0, 0x4820, 0x4c20, 0x8a40, 0x4980, 0x5660, 0xa000, 0x9fe0},
    {0xffe0, 0x8020, 0x8020, 0x9f20, 0x9120, 0x9120, 0x9120, 0x9f20, 0x8020, 0x8020, 0xffe0},
};

void drawCaptureScoreLabel(Adafruit_ST7735 &screen, const uint16_t (&label)[4][11], int top)
{
    constexpr int left = (CaptureAssets::screenWidth - (4 * 12 - 1)) / 2;
    for (int glyph = 0; glyph < 4; ++glyph)
        for (int row = 0; row < 11; ++row)
            for (int col = 0; col < 11; ++col)
                if (label[glyph][row] & (0x8000U >> col))
                    screen.writePixel(left + glyph * 12 + col, top + row, 0xffff);
}
}

bool CaptureScene::drawBands(Renderer &renderer, CaptureAssets &assets, const Frame &frame, uint32_t bands)
{
    constexpr int width = CaptureAssets::screenWidth;
    constexpr int rows = CaptureScene::bandRows;
    constexpr int spriteWidth = CaptureAssets::spriteMaxWidth;
    static_assert(width * rows + spriteWidth * rows <= FrameDecoder::kLineBufferPixels,
                  "Capture bands and sprite rows must fit the renderer workspace.");
    uint16_t *pixels = renderer.pixelWorkspace();
    uint16_t *spritePixels = pixels + width * rows;
    const auto &scene = assets.scene();

    for (int band = 0; band < CaptureScene::bandCount; ++band)
    {
        if ((bands & (1UL << band)) == 0) continue;
        const int top = band * rows;
        if (!assets.readBackground(top, rows, pixels)) return false;
        const int lineStart = frame.lineStart > top ? frame.lineStart : top;
        const int lineEnd = frame.lineEnd < top + rows ? frame.lineEnd : top + rows;
        for (int y = lineStart; y < lineEnd; ++y)
            pixels[(y - top) * width + frame.hookX] = 0xC6DA;

        for (uint8_t index = 0; index < frame.layerCount; ++index)
        {
            const auto &layer = frame.layers[index];
            const auto &sprite = *layer.sprite;
            const int y0 = layer.y > top ? layer.y : top;
            const int y1 = layer.y + sprite.height < top + rows ? layer.y + sprite.height : top + rows;
            const int x0 = layer.x > 0 ? layer.x : 0;
            const int x1 = layer.x + sprite.width < width ? layer.x + sprite.width : width;
            if (y1 <= y0 || x1 <= x0) continue;
            if (sprite.width > spriteWidth || !assets.readSprite(sprite, y0 - layer.y, y1 - y0, spritePixels))
                return false;

            for (int y = y0; y < y1; ++y)
            {
                const uint16_t *source = spritePixels + (y - y0) * sprite.width;
                uint16_t *destination = pixels + (y - top) * width;
                for (int x = x0; x < x1; ++x)
                {
                    const int local = x - layer.x;
                    const uint16_t pixel = source[layer.flip ? sprite.width - 1 - local : local];
                    if (pixel != sprite.transparent) destination[x] = pixel;
                }
            }
        }

        if (top < scene.timerY + CaptureScene::timerHeight && top + rows > scene.timerY)
            drawCaptureTimer(pixels, top, frame.secondsLeft, scene.timerX, scene.timerY);
        if (!renderer.writePixelBand(top, width, rows)) return false;
    }
    return true;
}

bool CaptureScene::showCard(Renderer &renderer, CaptureAssets &assets, const CaptureAssets::Card &card, uint8_t page)
{
    uint16_t *pixels = renderer.pixelWorkspace();
    for (uint8_t y = 0; y < CaptureAssets::screenHeight; y += CaptureScene::bandRows)
    {
        if (!assets.readCardRows(card, page, y, CaptureScene::bandRows, pixels) ||
            !renderer.writePixelBand(y, CaptureAssets::screenWidth, bandRows)) return false;
    }
    return true;
}

void CaptureScene::showScore(Renderer &renderer, int32_t score)
{
    auto &display = renderer.pixelDisplay();
    constexpr uint8_t minusGlyph = 11;
    uint32_t value = static_cast<uint32_t>(score);
    if (score < 0) value = 0U - value;
    uint8_t digits[11];
    int count = 0;
    do
    {
        digits[count++] = value % 10;
        value /= 10;
    } while (value);
    if (score < 0) digits[count++] = minusGlyph;

    const int width = count * 6 - 1;
    const int fittingScale = (CaptureAssets::screenWidth - 16) / width;
    const int scale = fittingScale < 4 ? fittingScale : 4;
    const int top = (CaptureAssets::screenHeight - 8 * scale) / 2;
    int left = (CaptureAssets::screenWidth - width * scale) / 2;
    display.fillScreen(0x11E9);
    display.startWrite();
    drawCaptureScoreLabel(display, captureScoreTitle, 26);
    for (int digit = count - 1; digit >= 0; --digit, left += 6 * scale)
    {
        const auto &glyph = NumberGlyphs::digits[digits[digit]];
        for (int row = 0; row < 8; ++row)
            for (int col = 0; col < 5; ++col)
                if (glyph[row] & (0x80U >> col))
                    display.writeFillRect(left + col * scale, top + row * scale, scale, scale, 0xffff);
    }
    drawCaptureScoreLabel(display, captureScoreReturn, 139);
    display.endWrite();
}
#endif
