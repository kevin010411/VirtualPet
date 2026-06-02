#include "render/Renderer.h"

#include <Arduino.h>
#include <Adafruit_ST7735.h>
#include <SdFat.h>
#include <string.h>
#include <vector>
#include "render/BMPRenderer.h"
#include "render/FrameDecoder.h"
#include "render/RLERenderer.h"
#include "render/RenderStatsReporter.h"

struct Renderer::AnimationState
{
    AnimationId nowAnimId = AnimationId::None;
    uint16_t animationIndex = 0;
    uint16_t maxFrame = 0;
    bool playOnce = false;
    char speciesCode[9] = "dino";
    char outfitCode[9] = "base";
    std::vector<uint8_t> rowBuffer;
    std::vector<uint16_t> lineBuffer;
    AssetManifest manifest;
#if ENABLE_RENDER_STATS
    RenderStats stats;
#endif
};

Renderer *Renderer::create(Adafruit_ST7735 *ref_tft, SdFat *ref_SD)
{
#if ENABLE_SD_RLE_ASSETS
    return new RLERenderer(ref_tft, ref_SD);
#elif ENABLE_SD_BMP_ASSETS
    return new BMPRenderer(ref_tft, ref_SD);
#endif
}

Renderer::Renderer(Adafruit_ST7735 *ref_tft, SdFat *ref_SD)
    : tft(ref_tft), SD(ref_SD), state(new AnimationState())
{
}

Renderer::~Renderer()
{
    delete state;
}

void Renderer::initAnimations()
{
    state->nowAnimId = AnimationId::None;
    state->animationIndex = 0;
    state->maxFrame = 0;
    state->playOnce = false;
#if ENABLE_RENDER_STATS
    state->stats = RenderStats{};
#endif
    reloadManifest();

    const int rowCapacity = ((FrameDecoder::kDefaultAnimWidth * 3 + 3) / 4) * 4 * FrameDecoder::kWorkingBatchLines;
    state->rowBuffer.assign(rowCapacity, 0);
    state->lineBuffer.assign(FrameDecoder::kDefaultAnimWidth * FrameDecoder::kWorkingBatchLines, 0);
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
    return state->manifest.load(SD, state->speciesCode, state->outfitCode);
}

Adafruit_ST7735 *Renderer::display() const
{
    return tft;
}

SdFat *Renderer::sdCard() const
{
    return SD;
}

std::vector<uint8_t> &Renderer::rowBuffer()
{
    return state->rowBuffer;
}

std::vector<uint16_t> &Renderer::lineBuffer()
{
    return state->lineBuffer;
}

bool Renderer::ShowSDCardImage(const char *img_path, int xmin, int ymin, int batch_lines)
{
    if (img_path == nullptr || img_path[0] == '\0')
    {
        FrameDecoder::showResourceError(tft);
        return false;
    }

    char resolvedPath[128];
    const bool hasPath = FrameDecoder::replaceOrAppendExtension(resolvedPath, sizeof(resolvedPath), img_path, assetExtension());
    const bool ok = hasPath && showImageFile(resolvedPath, xmin, ymin, batch_lines, nullptr);

    if (!ok)
        FrameDecoder::showResourceError(tft);
    return ok;
}

bool Renderer::ShowSDCardFrame(const char *base_path, uint16_t frame_index, int xmin, int ymin, int batch_lines)
{
    if (base_path == nullptr || base_path[0] == '\0')
    {
        FrameDecoder::showResourceError(tft);
        return false;
    }

    char candidatePath[128];
    if (!FrameDecoder::buildFramePath(candidatePath, sizeof(candidatePath), base_path, frame_index, assetExtension()))
    {
        FrameDecoder::showResourceError(tft);
        return false;
    }

    const bool ok = showImageFile(candidatePath, xmin, ymin, batch_lines, nullptr);
    if (!ok)
        FrameDecoder::showResourceError(tft);
    return ok;
}

bool Renderer::setAnimation(AnimationId id, bool playOnce)
{
    AnimationMeta *meta = state->manifest.metaFor(id);
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
    state->maxFrame = meta->frameCount;
    state->playOnce = playOnce;
    return true;
}

bool Renderer::advanceAnimationFrame()
{
    if (state->nowAnimId == AnimationId::None || state->maxFrame == 0)
        return true;

    if (state->animationIndex > state->maxFrame)
    {
        state->animationIndex = 1;
        if (state->playOnce)
            return true;
    }

#if ENABLE_RENDER_STATS
    const unsigned long frameStartUs = micros();
#endif
    AnimationMeta *meta = state->manifest.metaFor(state->nowAnimId);
    bool ok = false;

    if (!meta->configured || meta->frameCount == 0 || meta->path[0] == '\0')
    {
        FrameDecoder::showResourceError(tft);
    }
    else
    {
        char framePath[128];
        if (FrameDecoder::buildFramePath(framePath, sizeof(framePath), meta->path, state->animationIndex, assetExtension()))
            ok = showImageFile(framePath, 0, 32, FrameDecoder::kWorkingBatchLines, meta);
        if (!ok)
            FrameDecoder::showResourceError(tft);
    }

    if (ok)
    {
#if ENABLE_RENDER_STATS
        updateRenderStats(state->stats, SD, micros() - frameStartUs);
#endif
    }

    state->animationIndex++;
    return !ok;
}

unsigned long Renderer::frameIntervalFor(AnimationId id, unsigned long defaultIntervalMs) const
{
    const AnimationMeta *meta = state->manifest.metaFor(id);
    if (id == AnimationId::None || !meta->configured || meta->fpsHint == 0)
        return defaultIntervalMs;

    const unsigned long intervalMs = 1000UL / static_cast<unsigned long>(meta->fpsHint);
    return (intervalMs == 0) ? 1UL : intervalMs;
}
