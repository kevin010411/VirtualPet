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
#include "platform/TftDmaTransfer.h"

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

#if ENABLE_TRAINING_COURSES
void Renderer::showTrainingRecall(uint16_t choices, uint8_t count)
{
    constexpr uint16_t colors[] = {0xAAA8, 0xEF18, 0x53F3};
    uint8_t remaining = count > 5 ? 5 : count;
    while (remaining)
    {
        const uint8_t x = 21 + 19 * --remaining;
        tft->fillRect(x, 82, 1, 12, ST77XX_WHITE);
        tft->fillRect(x + 1, 82, 8, 6, colors[choices & 3]);
        choices >>= 2;
    }
}

void Renderer::showTrainingResult(uint8_t score, uint8_t total, bool passed)
{
    tft->fillScreen(ST77XX_BLACK);
    showInstructionFeedback(passed);
    const char display[] = {
        char('0' + score / 10), char('0' + score % 10), '/',
        char('0' + total / 10), char('0' + total % 10), '\0',
    };
    tft->setCursor(total < 10 ? 46 : 34, 117);
    tft->setTextColor(ST77XX_WHITE);
    tft->setTextSize(2);
    if (total < 10)
    {
        const char singleDigit[] = {display[1], '/', display[4], '\0'};
        tft->print(singleDigit);
    }
    else
        tft->print(display);
}
#endif
#endif

uint16_t *Renderer::pixelWorkspace() { return state->lineBuffer; }

bool Renderer::writePixelBand(uint8_t top, uint8_t width, uint8_t rows)
{
    uint16_t *pixels = pixelWorkspace();
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
