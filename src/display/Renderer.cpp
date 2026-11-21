#include "display/Renderer.h"

#include <Arduino.h>
#include <Adafruit_ST7735.h>
#include <SdFat.h>
#include <string.h>
#if ENABLE_DEBUG
#include <stdio.h>
#endif
#include "display/FrameDecoder.h"
#include "display/RenderStatsReporter.h"
#include "resources/AssetResources.h"
#if ENABLE_CAPTURE_GAME
#include "platform/TftDmaTransfer.h"
#endif

struct Renderer::AnimationState
{
    AssetData::AnimationRef animation = {};
    uint8_t versionIndex = 0;
    uint16_t nextFrame = 0;
    uint16_t maxFrame = 0;
    uint8_t layoutId = 0;
    bool fullScreen = false;
    bool playOnce = false;
    bool animationFrameFailed = false;
    uint8_t animationX = 0;
    uint8_t animationY = 32;
    uint8_t animationWidth = 128;
    uint8_t animationHeight = 96;
    alignas(uint64_t) uint16_t lineBuffer[FrameDecoder::kLineBufferPixels] = {};
#if ENABLE_DEBUG
    bool hasRenderedFrame = false;
#endif
};

namespace
{
AssetData::AssetFrameAddress frameAddress(const AssetData::AnimationRef &animation,
                                          uint8_t versionIndex,
                                          uint16_t zeroBasedFrame)
{
    AssetData::AssetFrameAddress address = {};
    address.speciesSlot = animation.speciesSlot;
    address.outfitSlot = animation.outfitSlot;
    address.animationId = animation.animationId;
    address.versionIndex = versionIndex;
    address.frameIndex = zeroBasedFrame;
    return address;
}
} // namespace

#if ENABLE_CAPTURE_GAME || ENABLE_SHOP
namespace
{
// Rows 2–9 of Fusion Pixel 12 px zh-TW. Each digit has 5 ink columns and a 6 px advance.
constexpr uint8_t numberGlyphs[][8] = {
    {0x70, 0x88, 0x98, 0xa8, 0xa8, 0xc8, 0x88, 0x70},
    {0x20, 0xe0, 0x20, 0x20, 0x20, 0x20, 0x20, 0xf8},
    {0x70, 0x88, 0x08, 0x10, 0x20, 0x40, 0x80, 0xf8},
    {0x70, 0x88, 0x08, 0x30, 0x08, 0x08, 0x88, 0x70},
    {0x30, 0x50, 0x50, 0x90, 0x90, 0xf8, 0x10, 0x10},
    {0xf8, 0x80, 0x80, 0xf0, 0x08, 0x08, 0x88, 0x70},
    {0x70, 0x88, 0x80, 0xf0, 0x88, 0x88, 0x88, 0x70},
    {0xf8, 0x08, 0x08, 0x10, 0x10, 0x20, 0x20, 0x20},
    {0x70, 0x88, 0x88, 0x70, 0x88, 0x88, 0x88, 0x70},
    {0x70, 0x88, 0x88, 0x88, 0x78, 0x08, 0x88, 0x70},
    {0x00, 0x00, 0x20, 0x20, 0xf8, 0x20, 0x20, 0x00}, // Shop overflow marker (+)
    {0x00, 0x00, 0x00, 0x00, 0xf8, 0x00, 0x00, 0x00}, // Score minus sign
};
}
#endif

Renderer::Renderer(Adafruit_ST7735 *ref_tft, AssetResources &resourcesRef, SPIClass *pixelSpi)
    : tft(ref_tft), resources(resourcesRef), pixelSpi(pixelSpi), state(new AnimationState)
#if ENABLE_DEBUG
      , debug(ref_tft)
#endif
{
}

#if ENABLE_CAPTURE_GAME
namespace
{
void drawCaptureTimer(uint16_t *pixels, uint16_t seconds)
{
    // Three columns by five rows, top-left bit first.
    constexpr uint16_t glyphs[] = {0x7b6f, 0x2c97, 0x73e7, 0x73cf, 0x5bc9,
                                   0x79cf, 0x79ef, 0x7292, 0x7bef, 0x7bcf, 0x0716};
    uint8_t digits[3];
    uint8_t count = 0;
    do
    {
        digits[count++] = seconds % 10;
        seconds /= 10;
    } while (seconds);
    for (int y = 0; y < 7; ++y)
    {
        uint16_t *row = pixels + y * CaptureAssets::screenWidth;
        for (int x = 0; x < 4 * (count + 1) + 1; ++x) row[x] = 0x1026;
    }
    for (uint8_t index = 0; index <= count; ++index)
    {
        const uint16_t glyph = glyphs[index == count ? 10 : digits[count - 1 - index]];
        for (int y = 0; y < 5; ++y)
            for (int x = 0; x < 3; ++x)
                if (glyph & (1U << (14 - 3 * y - x)))
                    pixels[(y + 1) * CaptureAssets::screenWidth + 1 + 4 * index + x] = 0xFFFF;
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

bool Renderer::drawCaptureBands(CaptureAssets &assets, const CaptureGame::Frame &frame, uint32_t bands)
{
    constexpr int width = CaptureAssets::screenWidth;
    constexpr int rows = CaptureGame::bandRows;
    constexpr int spriteWidth = CaptureAssets::spriteMaxWidth;
    static_assert(width * rows + spriteWidth * rows <= FrameDecoder::kLineBufferPixels,
                  "Capture bands and sprite rows must fit the renderer workspace.");
    uint16_t *pixels = state->lineBuffer;
    uint16_t *spritePixels = pixels + width * rows;

    for (int band = 0; band < CaptureGame::bandCount; ++band)
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

        if (band == 0) drawCaptureTimer(pixels, frame.secondsLeft);
        if (!sendCaptureBand(pixels, top)) return false;
    }
    return true;
}

bool Renderer::sendCaptureBand(uint16_t *pixels, uint8_t top)
{
    constexpr int width = CaptureAssets::screenWidth;
    constexpr int rows = CaptureGame::bandRows;
    tft->startWrite();
    tft->setAddrWindow(0, top, width, rows);
    bool sent = true;
#if defined(STM32F103xB)
    {
        TftDmaTransfer dma(pixelSpi);
        if (dma.active())
        {
            sent = dma.send(pixels, width * rows);
            sent = dma.finish() && sent;
        }
        else
            tft->writePixels(pixels, width * rows);
    }
#else
    tft->writePixels(pixels, width * rows);
#endif
    tft->endWrite();
    return sent;
}

bool Renderer::showCaptureCard(CaptureAssets &assets, const CaptureAssets::Card &card, uint8_t page)
{
    uint16_t *pixels = state->lineBuffer;
    for (uint8_t y = 0; y < CaptureAssets::screenHeight; y += CaptureGame::bandRows)
    {
        if (!assets.readCardRows(card, page, y, CaptureGame::bandRows, pixels) ||
            !sendCaptureBand(pixels, y)) return false;
    }
    return true;
}

void Renderer::showCaptureScore(int32_t score)
{
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
    tft->fillScreen(0x11E9);
    tft->startWrite();
    drawCaptureScoreLabel(*tft, captureScoreTitle, 26);
    for (int digit = count - 1; digit >= 0; --digit, left += 6 * scale)
    {
        const auto &glyph = numberGlyphs[digits[digit]];
        for (int row = 0; row < 8; ++row)
            for (int col = 0; col < 5; ++col)
                if (glyph[row] & (0x80U >> col))
                    tft->writeFillRect(left + col * scale, top + row * scale, scale, scale, 0xffff);
    }
    drawCaptureScoreLabel(*tft, captureScoreReturn, 139);
    tft->endWrite();
}
#endif

Renderer::~Renderer()
{
    delete state;
}

void Renderer::initAnimations()
{
    state->animation = {};
    state->versionIndex = 0;
    state->nextFrame = 0;
    state->maxFrame = 0;
    state->layoutId = 0;
    state->fullScreen = false;
    state->playOnce = false;
    state->animationFrameFailed = false;
}

void Renderer::setAnimationArea(uint8_t x, uint8_t y, uint8_t width, uint8_t height)
{
    state->animationX = x;
    state->animationY = y;
    state->animationWidth = width;
    state->animationHeight = height;
}

bool Renderer::ShowDataFrame(const AssetData::AssetFrameAddress &address,
                             int xmin,
                             int ymin,
                             int batch_lines, uint16_t expectedWidth, uint16_t expectedHeight,
                             const DisplayRect *crop)
{
    const bool ok = FrameDecoder::showDataFrame(
        resources.frameReader(), address, tft, resources.readBuffer(), AssetData::kDataReadBufferBytes,
        state->lineBuffer, FrameDecoder::kLineBufferPixels,
        xmin < 0 ? state->animationX : xmin,
        ymin < 0 ? state->animationY : ymin, batch_lines, expectedWidth, expectedHeight, pixelSpi, crop);
    if (!ok)
    {
        FrameDecoder::showAssetDataError(tft, resources.frameReader().firstErrorResource());
#if ENABLE_DEBUG
        showAssetDataDebug();
#endif
    }
#if ENABLE_DEBUG
    state->hasRenderedFrame |= ok;
#endif
    return ok;
}

#if ENABLE_PROJECT_MENUS
void Renderer::clearMenuBelow(uint8_t y)
{
    tft->fillRect(0, y, 128, 160 - y, ST77XX_BLACK);
}
#endif

bool Renderer::ShowAnimationFrame(const AssetData::AnimationRef &animation,
                                  uint8_t versionIndex,
                                  uint16_t frameIndex,
                                  int xmin,
                                  int ymin,
                                  int batchLines, uint16_t expectedWidth, uint16_t expectedHeight)
{
    if (!animation.valid() || frameIndex == 0)
    {
        FrameDecoder::showAssetDataError(tft, "asset reference");
#if ENABLE_DEBUG
        showAssetDataDebug("asset reference", "invalid asset ref");
#endif
        return false;
    }
    if (xmin < 0 && ymin < 0)
    {
        const auto &current = state->animation;
        if (state->fullScreen && versionIndex == state->versionIndex &&
            animation.animationId == current.animationId &&
            animation.speciesSlot == current.speciesSlot && animation.outfitSlot == current.outfitSlot)
        {
            xmin = ymin = 0;
            expectedWidth = 128;
            expectedHeight = 160;
        }
        else
        {
            if (state->animationWidth == 0)
                return true;
            expectedWidth = state->animationWidth;
            expectedHeight = state->animationHeight;
        }
    }
    return ShowDataFrame(frameAddress(animation, versionIndex,
                                      static_cast<uint16_t>(frameIndex - 1)),
                         xmin, ymin, batchLines, expectedWidth, expectedHeight);
}

bool Renderer::setAnimation(const AssetData::AnimationRef &animation,
                            uint8_t versionIndex,
                            bool playOnce)
{
    state->animationFrameFailed = false;
    state->animation = {};
    state->maxFrame = 0;
    state->fullScreen = false;
    AssetData::AnimationRecord record = {};
    if (!resources.resolveAnimation(animation, versionIndex, record))
        return false;
    state->animation = animation;
    state->versionIndex = versionIndex;
    state->nextFrame = 0;
    state->maxFrame = record.frameCount;
    state->layoutId = record.layoutId;
    state->fullScreen = record.fullScreen;
    state->playOnce = playOnce;
    return record.frameCount > 0;
}

bool Renderer::currentLayoutId(uint8_t &layoutId, bool *fullScreen) const
{
    if (!state->animation.valid() || state->maxFrame == 0)
        return false;
    layoutId = state->layoutId;
    if (fullScreen)
        *fullScreen = state->fullScreen;
    return true;
}

bool Renderer::willRestartAnimationLoop() const
{
    return state->maxFrame > 0 && state->nextFrame >= state->maxFrame;
}

bool Renderer::advanceAnimationFrame()
{
    if (!state->animation.valid() || state->maxFrame == 0)
    {
        state->animationFrameFailed = true;
        return true;
    }
    if (state->nextFrame >= state->maxFrame)
    {
        if (state->playOnce)
            return true;
        state->nextFrame = 0;
    }
    const bool ok = ShowAnimationFrame(
        state->animation, state->versionIndex, static_cast<uint16_t>(state->nextFrame + 1));
    if (ok)
        ++state->nextFrame;
    state->animationFrameFailed = !ok;
    return !ok;
}

bool Renderer::animationFrameFailed() const
{
    return state->animationFrameFailed;
}

#if ENABLE_STAT_EVENTS
void Renderer::showRoundComplete()
{
    tft->fillRect(0, 0, 128, 160, ST77XX_BLACK);
    tft->setTextSize(1);
    tft->setTextColor(0xffff, ST77XX_BLACK);
    tft->setCursor(22, 65);
    tft->print("Round complete");
    tft->setCursor(10, 85);
    tft->print("Confirm: new round");
}
#endif

void Renderer::showResourceError()
{
    FrameDecoder::showAssetDataError(tft, resources.firstAssetDataErrorResource());
#if ENABLE_DEBUG
    showAssetDataDebug(resources.firstAssetDataErrorResource());
#endif
}

void Renderer::showResourceError(const char *resource)
{
    FrameDecoder::showAssetDataError(tft, resource);
#if ENABLE_DEBUG
    showAssetDataDebug(resource);
#endif
}

#if ENABLE_DEBUG
void Renderer::showStartupResourceError(const char *fallbackResource,
                                        const char *debugStage)
{
    const char *recordedResource = resources.firstAssetDataErrorResource();
    const char *resource = recordedResource != nullptr && recordedResource[0] != '\0'
                               ? recordedResource
                               : fallbackResource;
    FrameDecoder::showAssetDataError(tft, resource);
    showAssetDataDebug(resource, debugStage);
}
#endif

#if ENABLE_DEBUG
DebugDisplay &Renderer::debugDisplay() { return debug; }

void Renderer::showAssetDataDebug(const char *fallbackResource,
                                  const char *debugStage)
{
    if (resources.frameReader().firstError() == AssetData::BundleError::None)
    {
        debug.showMessage(debugStage != nullptr && debugStage[0] != '\0'
                              ? debugStage
                              : "asset data error",
                          fallbackResource != nullptr && fallbackResource[0] != '\0'
                              ? fallbackResource
                              : "no reader detail");
        return;
    }

    const AssetData::AssetFrameAddress address = resources.frameReader().firstErrorAddress();
    char detail[21] = {};
    snprintf(detail, sizeof(detail), "s%u o%u a%u v%u f%u",
             address.speciesSlot, address.outfitSlot, address.animationId,
             address.versionIndex, address.frameIndex);
    debug.showMessage(resources.frameReader().firstErrorName(), detail);
}

void Renderer::renderDebugOverlay() { debug.render(); }
bool Renderer::hasRenderedFrame() const { return state->hasRenderedFrame; }
#endif

#if ENABLE_INSTRUCTION_RESPONSE
void Renderer::clearInstructionOptions() { tft->fillRect(0, 128, 128, 32, ST77XX_BLACK); }
void Renderer::showInstructionFeedback(bool correct)
{
    tft->fillRect(0, 32, 128, 96, ST77XX_BLACK);
    if (correct)
    {
        tft->drawLine(36, 80, 56, 100, ST77XX_GREEN);
        tft->drawLine(56, 100, 92, 56, ST77XX_GREEN);
    }
    else
    {
        tft->drawLine(40, 56, 88, 104, ST77XX_RED);
        tft->drawLine(88, 56, 40, 104, ST77XX_RED);
    }
}
#endif

#if ENABLE_SHOP
void Renderer::drawShopNumber(uint8_t x, uint8_t y, uint8_t width, uint8_t height, uint32_t value)
{
    if (!width) return;
    uint8_t digits[10];
    uint8_t count = 0;
    do {
        digits[count++] = value % 10;
        value /= 10;
    } while (value);
    int16_t left = width - count * 6;
    if (count * 6 > width)
    {
        digits[0] = 10;
        count = 1;
        left = 0;
    }
    const int16_t top = height / 2 - 4;
    uint16_t *line = state->lineBuffer;
    tft->startWrite();
    tft->setAddrWindow(x, y, width, height);
    for (uint8_t row = 0; row < height; ++row)
    {
        for (uint8_t col = 0; col < width; ++col) line[col] = 0x1126;
        const int16_t glyphRow = row - top;
        if (glyphRow >= 0 && glyphRow < 8)
        {
            int16_t pen = left;
            for (uint8_t digit = count; digit > 0; --digit, pen += 6)
            {
                const uint8_t bits = numberGlyphs[digits[digit - 1]][glyphRow];
                for (uint8_t col = 0; col < 5 && pen + col < width; ++col)
                    if (bits & (0x80 >> col)) line[pen + col] = 0xffff;
            }
        }
        tft->writePixels(line, width);
    }
    tft->endWrite();
}

void Renderer::highlightShopRow(uint8_t x, uint8_t y, uint8_t width, uint8_t height)
{
    tft->drawRect(x, y, width, height, 0x8758);
}
#endif
