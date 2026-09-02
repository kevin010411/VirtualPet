#include "presentation/adapters/rendering/Renderer.h"

#include <Arduino.h>
#include <Adafruit_ST7735.h>
#include <SdFat.h>
#include <string.h>
#include "presentation/adapters/rendering/FrameDecoder.h"
#include "presentation/adapters/rendering/RenderStatsReporter.h"


struct Renderer::AnimationState
{
    explicit AnimationState(SdFat *sd)
        : bundleReader(sd, readBuffer, AssetData::kIoScratchBytes)
    {
    }

    AssetData::AnimationRef animation = {};
    uint8_t versionIndex = 0;
    uint16_t nextFrame = 0;
    uint16_t maxFrame = 0;
    uint16_t frameMs = 0;
    bool playOnce = false;
    bool animationFrameFailed = false;
    char externalErrorResource[20] = {};
    uint8_t speciesSlot = 1;
    uint8_t outfitSlot = 1;
    alignas(uint16_t) uint8_t readBuffer[FrameDecoder::kDataReadBufferBytes] = {};
    BundleReader bundleReader;
    uint16_t lineBuffer[FrameDecoder::kLineBufferPixels] = {};
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

Renderer::Renderer(Adafruit_ST7735 *ref_tft, SdFat *ref_SD)
    : tft(ref_tft), SD(ref_SD), state(new AnimationState(ref_SD))
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
    state->frameMs = 0;
    state->playOnce = false;
    state->animationFrameFailed = false;
}

void Renderer::setAssetAppearance(uint8_t speciesSlot, uint8_t outfitSlot)
{
    if (speciesSlot != 0 && outfitSlot != 0)
    {
        state->speciesSlot = speciesSlot;
        state->outfitSlot = outfitSlot;
    }
}

bool Renderer::configureAssetBundle(const AssetData::BundleId &bundleId)
{
    return state->bundleReader.configureBundle(bundleId);
}

Adafruit_ST7735 *Renderer::display() const { return tft; }
SdFat *Renderer::sdCard() const { return SD; }
uint8_t *Renderer::readBuffer() { return state->readBuffer; }
size_t Renderer::readBufferSize() const { return sizeof(state->readBuffer); }
uint16_t *Renderer::lineBuffer() { return state->lineBuffer; }
size_t Renderer::lineBufferPixels() const
{
    return sizeof(state->lineBuffer) / sizeof(state->lineBuffer[0]);
}

bool Renderer::ShowDataFrame(const AssetData::AssetFrameAddress &address,
                             int xmin,
                             int ymin,
                             int batch_lines)
{
    const bool ok = FrameDecoder::showDataFrame(
        state->bundleReader, address, display(), readBuffer(), readBufferSize(),
        lineBuffer(), lineBufferPixels(), xmin, ymin, batch_lines);
    if (!ok)
        FrameDecoder::showAssetDataError(tft, state->bundleReader.firstErrorResource());
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
                                  int batchLines)
{
    if (!animation.valid() || frameIndex == 0)
    {
        FrameDecoder::showAssetDataError(tft, "asset reference");
        return false;
    }
    return ShowDataFrame(frameAddress(animation, versionIndex,
                                      static_cast<uint16_t>(frameIndex - 1)),
                         xmin, ymin, batchLines);
}

bool Renderer::setAnimation(const AssetData::AnimationRef &animation,
                            uint8_t versionIndex,
                            bool playOnce)
{
    state->animationFrameFailed = false;
    AssetData::AnimationRecord record = {};
    if (!animation.valid() ||
        !state->bundleReader.resolveAnimation(frameAddress(animation, versionIndex, 0), record))
        return false;
    state->animation = animation;
    state->versionIndex = versionIndex;
    state->nextFrame = 0;
    state->maxFrame = record.frameCount;
    state->frameMs = record.frameMs;
    state->playOnce = playOnce;
    return record.frameCount > 0;
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
    const bool ok = ShowAnimationFrame(state->animation, state->versionIndex,
                                       static_cast<uint16_t>(state->nextFrame + 1));
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
    const char *pack = state->bundleReader.firstErrorResource();
    FrameDecoder::showAssetDataError(
        tft, pack != nullptr && pack[0] != '\0' ? pack : state->externalErrorResource);
}

void Renderer::showResourceError(const char *resource)
{
    FrameDecoder::showAssetDataError(tft, resource);
}

void Renderer::recordAssetDataErrorResource(const char *resource)
{
    if (state->externalErrorResource[0] != '\0' || resource == nullptr || resource[0] == '\0')
        return;
    strncpy(state->externalErrorResource, resource, sizeof(state->externalErrorResource) - 1);
    state->externalErrorResource[sizeof(state->externalErrorResource) - 1] = '\0';
}

#if ENABLE_DEBUG
DebugDisplay &Renderer::debugDisplay() { return debug; }
void Renderer::renderDebugOverlay() { debug.render(); }
bool Renderer::hasRenderedFrame() const { return state->hasRenderedFrame; }
#endif

uint16_t Renderer::frameCountFor(const AssetData::AnimationRef &animation,
                                 uint8_t versionIndex)
{
    AssetData::AnimationRecord record = {};
    return animation.valid() &&
                   state->bundleReader.tryResolveAnimation(frameAddress(animation, versionIndex, 0), record)
               ? record.frameCount
               : 0;
}

uint8_t Renderer::versionCountFor(const AssetData::AnimationRef &animation)
{
    uint8_t count = 0;
    for (uint8_t version = 0; version < AssetData::kMaxVersions; ++version)
    {
        AssetData::AnimationRecord record = {};
        if (!animation.valid() ||
            !state->bundleReader.tryResolveAnimation(frameAddress(animation, version, 0), record))
            break;
        ++count;
    }
    return count;
}

unsigned long Renderer::frameIntervalFor(const AssetData::AnimationRef &animation,
                                         uint8_t versionIndex,
                                         unsigned long defaultIntervalMs)
{
    AssetData::AnimationRecord record = {};
    if (!animation.valid() ||
        !state->bundleReader.tryResolveAnimation(frameAddress(animation, versionIndex, 0), record) ||
        record.frameMs == 0)
        return defaultIntervalMs;
    return max(1UL, static_cast<unsigned long>(record.frameMs));
}

AssetData::BundleError Renderer::firstAssetDataError() const
{
    return state->bundleReader.firstError();
}

const char *Renderer::firstAssetDataErrorResource() const
{
    const char *pack = state->bundleReader.firstErrorResource();
    return pack != nullptr && pack[0] != '\0' ? pack : state->externalErrorResource;
}
