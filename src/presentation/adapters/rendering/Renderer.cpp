#include "presentation/adapters/rendering/Renderer.h"

#include <Arduino.h>
#include <Adafruit_ST7735.h>
#include <SdFat.h>
#include <string.h>
#include "presentation/adapters/rendering/FrameDecoder.h"
#include "presentation/adapters/rendering/RenderStatsReporter.h"

struct Renderer::AnimationState
{
    AnimationId nowAnimId = AnimationId::None;
    uint16_t animationIndex = 0;
    uint16_t maxFrame = 0;
    bool playOnce = false;
    bool animationFrameFailed = false;
    char speciesCode[9] = "dino";
    char outfitCode[9] = "base";
    uint8_t readBuffer[FrameDecoder::kRleReadBufferBytes] = {};
    uint16_t lineBuffer[FrameDecoder::kLineBufferPixels] = {};
    AssetManifest manifest;
    const AnimationMeta *namedAnimationMeta = nullptr;
#if ENABLE_DEBUG
    RenderStats stats;
    bool hasRenderedFrame = false;
#endif
};

Renderer::Renderer(Adafruit_ST7735 *ref_tft, SdFat *ref_SD)
    : tft(ref_tft), SD(ref_SD), state(new AnimationState())
#if ENABLE_DEBUG
      , debug(ref_tft)
#endif
{
}

Renderer::~Renderer()
{
    delete state;
}

const char *Renderer::assetExtension() const
{
    return ".rle";
}

bool Renderer::showImageFile(const char *imgPath,
                             int xmin,
                             int ymin,
                             int batchLines)
{
    return FrameDecoder::showRleImage(
        sdCard(), display(), readBuffer(), readBufferSize(), lineBuffer(), lineBufferPixels(),
        imgPath, xmin, ymin, batchLines);
}

bool Renderer::showFramePath(const char *path, int xmin, int ymin, int batchLines)
{
    const bool ok = path != nullptr && showImageFile(path, xmin, ymin, batchLines);
    if (!ok)
        FrameDecoder::showResourceError(tft);
    return ok;
}

bool Renderer::showManifestFrame(const AnimationMeta *meta, uint16_t frameIndex, int xmin, int ymin, int batchLines)
{
    if (meta == nullptr || !meta->configured || meta->frameCount == 0 || meta->path[0] == '\0')
        return showFramePath(nullptr, xmin, ymin, batchLines);

    const uint16_t maxFrame = meta->singleFile ? 1 : meta->frameCount;
    const uint16_t safeFrame = frameIndex < 1 ? 1 : (frameIndex > maxFrame ? maxFrame : frameIndex);
    char framePath[128];
    const bool hasFramePath = meta->singleFile
                                  ? FrameDecoder::replaceOrAppendExtension(framePath, sizeof(framePath), meta->path, assetExtension())
                                  : FrameDecoder::buildFramePath(framePath, sizeof(framePath), meta->path, safeFrame, assetExtension());
    return showFramePath(hasFramePath ? framePath : nullptr, xmin, ymin, batchLines);
}

void Renderer::initAnimations()
{
    state->nowAnimId = AnimationId::None;
    state->animationIndex = 0;
    state->maxFrame = 0;
    state->playOnce = false;
    state->animationFrameFailed = false;
#if ENABLE_DEBUG
    state->stats = RenderStats{};
#endif
    reloadManifest();

}

void Renderer::setAssetAppearance(const char *speciesCode, const char *outfitCode)
{
    if (speciesCode == nullptr || speciesCode[0] == '\0')
    {
        strncpy(state->speciesCode, "dino", sizeof(state->speciesCode) - 1);
    }
    else
    {
        strncpy(state->speciesCode, speciesCode, sizeof(state->speciesCode) - 1);
    }
    state->speciesCode[sizeof(state->speciesCode) - 1] = '\0';

    if (outfitCode == nullptr || outfitCode[0] == '\0')
    {
        strncpy(state->outfitCode, "base", sizeof(state->outfitCode) - 1);
    }
    else
    {
        strncpy(state->outfitCode, outfitCode, sizeof(state->outfitCode) - 1);
    }
    state->outfitCode[sizeof(state->outfitCode) - 1] = '\0';
}

bool Renderer::reloadManifest()
{
    state->manifest.reset();
    state->animationIndex = 1;
    state->maxFrame = 0;
    const bool loaded = state->manifest.load(SD, state->speciesCode, state->outfitCode);
    if (!loaded || state->manifest.hasPathError() || state->manifest.hasCapacityError())
        FrameDecoder::showResourceError(tft);
    return loaded && !state->manifest.hasPathError() && !state->manifest.hasCapacityError();
}

Adafruit_ST7735 *Renderer::display() const
{
    return tft;
}

SdFat *Renderer::sdCard() const
{
    return SD;
}

uint8_t *Renderer::readBuffer()
{
    return state->readBuffer;
}

size_t Renderer::readBufferSize() const
{
    return sizeof(state->readBuffer);
}

uint16_t *Renderer::lineBuffer()
{
    return state->lineBuffer;
}

size_t Renderer::lineBufferPixels() const
{
    return sizeof(state->lineBuffer) / sizeof(state->lineBuffer[0]);
}

bool Renderer::ShowSDCardFrame(const char *base_path, uint16_t frame_index, int xmin, int ymin, int batch_lines)
{
    if (base_path == nullptr || base_path[0] == '\0')
        return showFramePath(nullptr, xmin, ymin, batch_lines);

    char candidatePath[128];
    const bool hasFramePath = FrameDecoder::buildFramePath(candidatePath, sizeof(candidatePath), base_path, frame_index, assetExtension());
    return showFramePath(hasFramePath ? candidatePath : nullptr, xmin, ymin, batch_lines);
}

bool Renderer::ShowAnimationFrame(AnimationId id, uint16_t frame_index, int xmin, int ymin, int batch_lines)
{
    return id == AnimationId::None
               ? showFramePath(nullptr, xmin, ymin, batch_lines)
               : showManifestFrame(state->manifest.metaFor(id), frame_index, xmin, ymin, batch_lines);
}

bool Renderer::ShowNamedAnimationFrame(const char *name, uint16_t frame_index, int xmin, int ymin, int batch_lines)
{
    return name == nullptr || name[0] == '\0'
               ? showFramePath(nullptr, xmin, ymin, batch_lines)
               : showManifestFrame(state->manifest.metaForName(name), frame_index, xmin, ymin, batch_lines);
}

bool Renderer::setAnimation(AnimationId id, bool playOnce)
{
    state->animationFrameFailed = false;
    state->namedAnimationMeta = nullptr;
    const AnimationMeta *meta = state->manifest.metaFor(id);
    if (id == AnimationId::None || !meta->configured || meta->frameCount == 0 || meta->path[0] == '\0')
    {
        state->nowAnimId = id;
        state->animationIndex = 1;
        state->maxFrame = 1;
        state->playOnce = true;
        return true;
    }

    state->nowAnimId = id;
    state->animationIndex = 1;
    state->maxFrame = meta->singleFile ? 1 : meta->frameCount;
    state->playOnce = playOnce;
    return true;
}

bool Renderer::setNamedAnimation(const char *name, bool playOnce)
{
    state->animationFrameFailed = false;
    const AnimationMeta *meta = state->manifest.metaForName(name);
    if (name == nullptr || name[0] == '\0' || meta == nullptr || !meta->configured || meta->frameCount == 0 || meta->path[0] == '\0')
    {
        state->namedAnimationMeta = nullptr;
        state->nowAnimId = AnimationId::None;
        state->animationIndex = 1;
        state->maxFrame = 1;
        state->playOnce = true;
        return true;
    }

    state->namedAnimationMeta = meta;
    state->nowAnimId = AnimationId::None;
    state->animationIndex = 1;
    state->maxFrame = meta->singleFile ? 1 : meta->frameCount;
    state->playOnce = playOnce;
    return true;
}

bool Renderer::willRestartAnimationLoop() const
{
    return state->animationIndex > state->maxFrame;
}

bool Renderer::advanceAnimationFrame()
{
    if ((state->nowAnimId == AnimationId::None && state->namedAnimationMeta == nullptr) || state->maxFrame == 0)
    {
        state->animationFrameFailed = true;
        return true;
    }

    if (state->animationIndex > state->maxFrame)
    {
        state->animationIndex = 1;
        if (state->playOnce)
        {
            state->animationFrameFailed = false;
            return true;
        }
    }

#if ENABLE_DEBUG
    const unsigned long frameStartUs = micros();
#endif
    const AnimationMeta *meta = state->namedAnimationMeta != nullptr
                                    ? state->namedAnimationMeta
                                    : state->manifest.metaFor(state->nowAnimId);
    bool ok = false;

    ok = showManifestFrame(meta, state->animationIndex, 0, 32, FrameDecoder::kWorkingBatchLines);

    if (ok)
    {
#if ENABLE_DEBUG
        updateRenderStats(state->stats, SD, micros() - frameStartUs);
        state->hasRenderedFrame = true;
#endif
    }

    state->animationIndex++;
    state->animationFrameFailed = !ok;
    return !ok;
}

bool Renderer::animationFrameFailed() const
{
    return state->animationFrameFailed;
}

void Renderer::showResourceError()
{
    FrameDecoder::showResourceError(tft);
}

#if ENABLE_DEBUG
DebugDisplay &Renderer::debugDisplay()
{
    return debug;
}

void Renderer::renderDebugOverlay()
{
    debug.render();
}

bool Renderer::hasRenderedFrame() const
{
    return state->hasRenderedFrame;
}
#endif

uint16_t Renderer::frameCountFor(AnimationId id) const
{
    const AnimationMeta *meta = state->manifest.metaFor(id);
    if (id == AnimationId::None || !meta->configured || meta->frameCount == 0)
        return 0;

    return meta->singleFile ? 1 : meta->frameCount;
}

uint16_t Renderer::frameCountForName(const char *name) const
{
    const AnimationMeta *meta = state->manifest.metaForName(name);
    if (name == nullptr || name[0] == '\0' || meta == nullptr || !meta->configured || meta->frameCount == 0)
        return 0;

    return meta->singleFile ? 1 : meta->frameCount;
}

uint8_t Renderer::variantCountFor(const char *baseName) const
{
    return state->manifest.variantCountFor(baseName);
}

const char *Renderer::variantNameFor(const char *baseName, uint8_t index) const
{
    return state->manifest.variantNameFor(baseName, index);
}

unsigned long Renderer::frameIntervalFor(AnimationId id, unsigned long defaultIntervalMs) const
{
    const AnimationMeta *meta = state->manifest.metaFor(id);
    if (id == AnimationId::None || !meta->configured || meta->frameIntervalMs == 0)
        return defaultIntervalMs;

    return max(1UL, static_cast<unsigned long>(meta->frameIntervalMs));
}

unsigned long Renderer::frameIntervalForName(const char *name, unsigned long defaultIntervalMs) const
{
    const AnimationMeta *meta = state->manifest.metaForName(name);
    if (name == nullptr || name[0] == '\0' || meta == nullptr || !meta->configured || meta->frameIntervalMs == 0)
        return defaultIntervalMs;

    return max(1UL, static_cast<unsigned long>(meta->frameIntervalMs));
}
