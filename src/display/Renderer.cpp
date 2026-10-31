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

struct Renderer::AnimationState
{
    AssetData::AnimationRef animation = {};
    uint8_t versionIndex = 0;
    uint16_t nextFrame = 0;
    uint16_t maxFrame = 0;
    uint8_t layoutId = 0;
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

Renderer::Renderer(Adafruit_ST7735 *ref_tft, AssetResources &resourcesRef)
    : tft(ref_tft), resources(resourcesRef), state(new AnimationState)
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
        ymin < 0 ? state->animationY : ymin, batch_lines, expectedWidth, expectedHeight);
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
        if (state->animationWidth == 0)
            return true;
        expectedWidth = state->animationWidth;
        expectedHeight = state->animationHeight;
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
    AssetData::AnimationRecord record = {};
    if (!resources.resolveAnimation(animation, versionIndex, record))
        return false;
    state->animation = animation;
    state->versionIndex = versionIndex;
    state->nextFrame = 0;
    state->maxFrame = record.frameCount;
    state->layoutId = record.layoutId;
    state->playOnce = playOnce;
    return record.frameCount > 0;
}

bool Renderer::currentLayoutId(uint8_t &layoutId) const
{
    if (!state->animation.valid() || state->maxFrame == 0)
        return false;
    layoutId = state->layoutId;
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
    const bool ok = state->animationWidth == 0 || ShowAnimationFrame(
        state->animation, state->versionIndex, static_cast<uint16_t>(state->nextFrame + 1),
        state->animationX, state->animationY, FrameDecoder::kWorkingBatchLines,
        state->animationWidth, state->animationHeight);
    if (ok)
        ++state->nextFrame;
    state->animationFrameFailed = !ok;
    return !ok;
}

bool Renderer::animationFrameFailed() const
{
    return state->animationFrameFailed;
}

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
