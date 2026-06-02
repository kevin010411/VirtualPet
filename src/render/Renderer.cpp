#include "render/Renderer.h"

#include <Arduino.h>
#include <Adafruit_ST7735.h>
#include <SdFat.h>
#include <string.h>
#include <vector>
#include "render/AssetManifest.h"
#include "render/FrameDecoder.h"
#include "render/RenderStatsReporter.h"

struct Renderer::AnimationState
{
    AnimationId nowAnimId = AnimationId::None;
    uint16_t animationIndex = 0;
    uint16_t maxFrame = 0;
    bool playOnce = false;
    Renderer::AssetFormatPreference formatPreference = Renderer::AssetFormatPreference::Auto;
    char speciesCode[9] = "dino";
    char outfitCode[9] = "base";
    std::vector<uint8_t> rowBuffer;
    std::vector<uint16_t> lineBuffer;
    AssetManifest manifest;
    RenderStats stats;
};

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
    state->stats = RenderStats{};
    reloadManifest();

    const int rowCapacity = ((FrameDecoder::kDefaultAnimWidth * 3 + 3) / 4) * 4 * FrameDecoder::kWorkingBatchLines;
    state->rowBuffer.assign(rowCapacity, 0);
    state->lineBuffer.assign(FrameDecoder::kDefaultAnimWidth * FrameDecoder::kWorkingBatchLines, 0);
}

void Renderer::setAssetFormatPreference(AssetFormatPreference preference)
{
    state->formatPreference = preference;
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

bool Renderer::ShowSDCardImage(const char *img_path, int xmin, int ymin, int batch_lines)
{
    if (img_path == nullptr || img_path[0] == '\0')
    {
        FrameDecoder::showResourceError(tft);
        return false;
    }

    bool ok = false;
    if (FrameDecoder::endsWithIgnoreCase(img_path, ".bmp"))
    {
        ok = FrameDecoder::showBmpImage(SD, tft, state->rowBuffer, state->lineBuffer, img_path, xmin, ymin, batch_lines);
    }
    else if (FrameDecoder::endsWithIgnoreCase(img_path, ".rle"))
    {
        ok = FrameDecoder::showRleImage(SD, tft, state->lineBuffer, img_path, 0, 0, false, xmin, ymin);
    }
    else if (FrameDecoder::endsWithIgnoreCase(img_path, ".raw"))
    {
        ok = FrameDecoder::showRawImage(SD, tft, state->lineBuffer, img_path, FrameDecoder::kDefaultAnimWidth, FrameDecoder::kDefaultAnimHeight, xmin, ymin, batch_lines);
    }
    else
    {
        char resolvedPath[128];
        if (FrameDecoder::replaceOrAppendExtension(resolvedPath, sizeof(resolvedPath), img_path, ".bmp"))
            ok = FrameDecoder::showBmpImage(SD, tft, state->rowBuffer, state->lineBuffer, resolvedPath, xmin, ymin, batch_lines);
        if (!ok && FrameDecoder::replaceOrAppendExtension(resolvedPath, sizeof(resolvedPath), img_path, ".rle"))
            ok = FrameDecoder::showRleImage(SD, tft, state->lineBuffer, resolvedPath, 0, 0, false, xmin, ymin);
    }

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
    bool ok = false;

    const char *primaryExt = ".bmp";
    const char *fallbackExt = ".rle";
    if (state->formatPreference == AssetFormatPreference::PreferRle)
    {
        primaryExt = ".rle";
        fallbackExt = ".bmp";
    }

    if (FrameDecoder::buildFramePath(candidatePath, sizeof(candidatePath), base_path, frame_index, primaryExt) &&
        FrameDecoder::fileExists(SD, candidatePath))
    {
        ok = ShowSDCardImage(candidatePath, xmin, ymin, batch_lines);
    }
    else if (FrameDecoder::buildFramePath(candidatePath, sizeof(candidatePath), base_path, frame_index, fallbackExt) &&
             FrameDecoder::fileExists(SD, candidatePath))
    {
        ok = ShowSDCardImage(candidatePath, xmin, ymin, batch_lines);
    }
    else if (FrameDecoder::buildFramePath(candidatePath, sizeof(candidatePath), base_path, frame_index, primaryExt))
    {
        ok = ShowSDCardImage(candidatePath, xmin, ymin, batch_lines);
    }

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

    const unsigned long frameStartUs = micros();
    AnimationMeta *meta = state->manifest.metaFor(state->nowAnimId);
    bool ok = false;

    if (!meta->configured || meta->frameCount == 0 || meta->path[0] == '\0')
    {
        FrameDecoder::showResourceError(tft);
    }
    else if (meta->format == AssetFormat::RawRgb565Sequence)
    {
        ok = FrameDecoder::showRawFrame(SD, tft, state->lineBuffer, *meta, state->animationIndex);
        if (!ok)
            FrameDecoder::showResourceError(tft);
    }
    else
    {
        char primaryPath[128];
        char fallbackPath[128];
        const char *primaryExt = ".bmp";
        const char *fallbackExt = ".rle";

        if (meta->format == AssetFormat::BmpSequence)
        {
            if (state->formatPreference == AssetFormatPreference::PreferRle)
            {
                primaryExt = ".rle";
                fallbackExt = ".bmp";
            }
        }
        else
        {
            primaryExt = ".rle";
            fallbackExt = ".bmp";
            if (state->formatPreference == AssetFormatPreference::PreferBmp)
            {
                primaryExt = ".bmp";
                fallbackExt = ".rle";
            }
        }

        if (FrameDecoder::buildFramePath(primaryPath, sizeof(primaryPath), meta->path, state->animationIndex, primaryExt))
        {
            if (FrameDecoder::endsWithIgnoreCase(primaryPath, ".rle"))
                ok = FrameDecoder::showRleImage(SD, tft, state->lineBuffer, primaryPath, meta->width, meta->height, true, 0, 32);
            else
                ok = ShowSDCardImage(primaryPath, 0, 32);
        }

        if (!ok &&
            FrameDecoder::buildFramePath(fallbackPath, sizeof(fallbackPath), meta->path, state->animationIndex, fallbackExt) &&
            FrameDecoder::fileExists(SD, fallbackPath))
        {
            if (FrameDecoder::endsWithIgnoreCase(fallbackPath, ".rle"))
                ok = FrameDecoder::showRleImage(SD, tft, state->lineBuffer, fallbackPath, meta->width, meta->height, true, 0, 32);
            else
                ok = ShowSDCardImage(fallbackPath, 0, 32);
        }
    }

    if (ok)
        updateRenderStats(state->stats, SD, micros() - frameStartUs);

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
