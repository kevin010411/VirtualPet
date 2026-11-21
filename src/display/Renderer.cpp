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
    tft->fillScreen(0x11E9);
    tft->setTextColor(0xFFFF);
    tft->setTextSize(2);
    tft->setCursor(34, 45);
    tft->print("SCORE");
    tft->setCursor(28, 73);
    tft->print(static_cast<long>(score));
    tft->setTextSize(1);
    tft->setCursor(28, 121);
    tft->print("OK TO RETURN");
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
                             int batch_lines, uint16_t expectedWidth, uint16_t expectedHeight)
{
    const bool ok = FrameDecoder::showDataFrame(
        resources.frameReader(), address, tft, resources.readBuffer(), AssetData::kDataReadBufferBytes,
        state->lineBuffer, FrameDecoder::kLineBufferPixels,
        xmin < 0 ? state->animationX : xmin,
        ymin < 0 ? state->animationY : ymin, batch_lines, expectedWidth, expectedHeight, pixelSpi);
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
    // Rows 2–9 of Fusion Pixel 12 px zh-TW, from fusion-pixel-12-zh-tw.bin.
    // These glyphs all have a six-pixel advance and a five-pixel bitmap.
    static constexpr uint8_t glyphs[][8] = {
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
        {0x00, 0x00, 0x20, 0x20, 0xf8, 0x20, 0x20, 0x00},
    };
    uint8_t digits[10];
    uint8_t count = 0;
    do {
        digits[count++] = value % 10;
        value /= 10;
    } while (value);
    int16_t pen = x + width - count * 6;
    if (count * 6 > width)
    {
        digits[0] = 10;
        count = 1;
        pen = x;
    }
    const int16_t top = y + height / 2 - 4;
    tft->startWrite();
    tft->writeFillRect(x, y, width, height, 0x1126);
    while (count)
    {
        const auto &glyph = glyphs[digits[--count]];
        for (uint8_t row = 0; row < 8; ++row)
        {
            if (top + row < y || top + row >= y + height) continue;
            for (uint8_t col = 0; col < 5 && pen + col < x + width; ++col)
            {
                if (glyph[row] & (0x80 >> col)) tft->writePixel(pen + col, top + row, 0xffff);
            }
        }
        pen += 6;
    }
    tft->endWrite();
}

void Renderer::highlightShopRow(uint8_t x, uint8_t y, uint8_t width, uint8_t height)
{
    tft->drawRect(x, y, width, height, 0x8758);
}
#endif
