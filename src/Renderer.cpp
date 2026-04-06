#include "Renderer.h"

#include <Arduino.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vector>

namespace
{
    enum class AssetFormat : uint8_t
    {
        BmpSequence,
        RawRgb565Sequence
    };

    struct AnimationMeta
    {
        char path[96];
        AssetFormat format;
        uint16_t width;
        uint16_t height;
        uint16_t frameCount;
        uint8_t fpsHint;
        bool configured;
    };

    struct RenderStats
    {
        uint32_t totalFrames = 0;
        uint64_t totalRenderTimeUs = 0;
        uint32_t windowFrames = 0;
        uint64_t windowRenderTimeUs = 0;
        unsigned long lastReportMs = 0;
        unsigned long lastFrameMs = 0;
        uint32_t lastFrameUs = 0;
        float avgFpsTotal = 0.0f;
        float avgFpsWindow = 0.0f;
    };

    constexpr uint16_t kDefaultAnimWidth = 128;
    constexpr uint16_t kDefaultAnimHeight = 96;
    constexpr uint16_t kWorkingBatchLines = 4;
    constexpr const char *kManifestPath = "/index.txt";

    AnimationMeta gAnimationRegistry[kAnimationIdCount] = {};

    AnimationMeta *metaFor(AnimationId id)
    {
        const size_t index = static_cast<size_t>(id);
        if (index >= kAnimationIdCount)
            return &gAnimationRegistry[0];
        return &gAnimationRegistry[index];
    }

    void trimWhitespace(char *text)
    {
        if (text == nullptr)
            return;

        char *start = text;
        while (*start == ' ' || *start == '\t')
            ++start;

        if (start != text)
            memmove(text, start, strlen(start) + 1);

        size_t len = strlen(text);
        while (len > 0 && (text[len - 1] == ' ' || text[len - 1] == '\t' || text[len - 1] == '\r' || text[len - 1] == '\n'))
        {
            text[len - 1] = '\0';
            --len;
        }
    }

    AssetFormat parseAssetFormat(const char *text)
    {
        if (strcmp(text, "raw") == 0)
            return AssetFormat::RawRgb565Sequence;
        return AssetFormat::BmpSequence;
    }

    bool splitManifestFields(char *line, char **fields, size_t fieldCount)
    {
        size_t count = 0;
        char *cursor = line;

        while (count < fieldCount)
        {
            fields[count++] = cursor;
            char *sep = strchr(cursor, '|');
            if (sep == nullptr)
                break;

            *sep = '\0';
            cursor = sep + 1;
        }

        if (count != fieldCount)
            return false;

        if (strchr(fields[fieldCount - 1], '|') != nullptr)
            return false;

        for (size_t i = 0; i < fieldCount; ++i)
        {
            trimWhitespace(fields[i]);
            if (fields[i][0] == '\0')
                return false;
        }
        return true;
    }

    void showResourceError(Adafruit_ST7735 *tft)
    {
        if (tft == nullptr)
            return;

        tft->fillRect(0, 32, kDefaultAnimWidth, kDefaultAnimHeight, ST77XX_BLACK);
        tft->setTextColor(ST77XX_RED, ST77XX_BLACK);
        tft->setCursor(8, 72);
        tft->print("resource error");
    }

    bool loadManifest(SdFat *sd)
    {
        if (sd == nullptr)
            return false;

        File manifest = sd->open(kManifestPath, FILE_READ);
        if (!manifest)
            return false;

        char line[256];
        while (manifest.available())
        {
            const int len = manifest.readBytesUntil('\n', line, sizeof(line) - 1);
            line[len] = '\0';
            trimWhitespace(line);

            if (line[0] == '\0' || line[0] == '#')
                continue;

            char *fields[7] = {};
            if (!splitManifestFields(line, fields, 7))
                continue;

            const AnimationId targetId = animationIdFromName(fields[0]);
            if (targetId == AnimationId::None)
                continue;

            const bool isBmp = strcmp(fields[1], "bmp") == 0;
            const bool isRaw = strcmp(fields[1], "raw") == 0;
            if (!isBmp && !isRaw)
                continue;

            AnimationMeta parsed = {};
            parsed.format = parseAssetFormat(fields[1]);
            parsed.frameCount = static_cast<uint16_t>(strtoul(fields[2], nullptr, 10));
            parsed.width = static_cast<uint16_t>(strtoul(fields[3], nullptr, 10));
            parsed.height = static_cast<uint16_t>(strtoul(fields[4], nullptr, 10));
            parsed.fpsHint = static_cast<uint8_t>(strtoul(fields[5], nullptr, 10));
            strncpy(parsed.path, fields[6], sizeof(parsed.path) - 1);
            parsed.path[sizeof(parsed.path) - 1] = '\0';

            if (parsed.frameCount == 0 || parsed.width == 0 || parsed.height == 0 || parsed.path[0] == '\0')
                continue;

            parsed.configured = true;
            *metaFor(targetId) = parsed;
        }

        manifest.close();
        return true;
    }
} // namespace

struct Renderer::AnimationState
{
    AnimationId nowAnimId = AnimationId::None;
    uint16_t animationIndex = 0;
    uint16_t maxFrame = 0;
    bool playOnce = false;
    std::vector<uint8_t> rowBuffer;
    std::vector<uint16_t> lineBuffer;
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

static bool writeFpsReport(SdFat *sd, const RenderStats &stats)
{
    if (sd == nullptr)
        return false;

    File f = sd->open("/render_fps.tmp", FILE_WRITE);
    if (!f)
        return false;

    char line[96];
    snprintf(line, sizeof(line), "total_frames=%lu\n", static_cast<unsigned long>(stats.totalFrames));
    f.print(line);
    snprintf(line, sizeof(line), "avg_fps_total=%.2f\n", stats.avgFpsTotal);
    f.print(line);
    snprintf(line, sizeof(line), "avg_fps_window=%.2f\n", stats.avgFpsWindow);
    f.print(line);
    snprintf(line, sizeof(line), "last_frame_ms=%lu\n", stats.lastFrameMs);
    f.print(line);
    snprintf(line, sizeof(line), "last_frame_us=%lu\n", static_cast<unsigned long>(stats.lastFrameUs));
    f.print(line);
    f.flush();
    f.close();

    if (sd->exists("/render_fps.txt"))
        sd->remove("/render_fps.txt");
    return sd->rename("/render_fps.tmp", "/render_fps.txt");
}

static void updateRenderStats(RenderStats &stats, SdFat *sd, uint32_t frameRenderUs)
{
    stats.totalFrames += 1;
    stats.windowFrames += 1;
    stats.totalRenderTimeUs += frameRenderUs;
    stats.windowRenderTimeUs += frameRenderUs;
    stats.lastFrameUs = frameRenderUs;
    stats.lastFrameMs = millis();

    if (stats.totalRenderTimeUs > 0)
        stats.avgFpsTotal = (1000000.0f * stats.totalFrames) / static_cast<float>(stats.totalRenderTimeUs);
    if (stats.windowRenderTimeUs > 0)
        stats.avgFpsWindow = (1000000.0f * stats.windowFrames) / static_cast<float>(stats.windowRenderTimeUs);

    const bool reportDueByTime = (stats.lastFrameMs - stats.lastReportMs) >= 5000UL;
    const bool reportDueByFrames = stats.windowFrames >= 100;
    if (!reportDueByTime && !reportDueByFrames)
        return;

    writeFpsReport(sd, stats);
    stats.lastReportMs = stats.lastFrameMs;
    stats.windowFrames = 0;
    stats.windowRenderTimeUs = 0;
}

void Renderer::initAnimations()
{
    state->nowAnimId = AnimationId::None;
    state->animationIndex = 0;
    state->maxFrame = 0;
    state->playOnce = false;
    state->stats = RenderStats{};

    for (size_t i = 0; i < kAnimationIdCount; ++i)
    {
        AnimationMeta &meta = gAnimationRegistry[i];
        meta.path[0] = '\0';
        meta.format = AssetFormat::BmpSequence;
        meta.width = kDefaultAnimWidth;
        meta.height = kDefaultAnimHeight;
        meta.frameCount = 0;
        meta.fpsHint = 0;
        meta.configured = false;
    }

    loadManifest(SD);

    const int rowCapacity = ((kDefaultAnimWidth * 3 + 3) / 4) * 4 * kWorkingBatchLines;
    state->rowBuffer.assign(rowCapacity, 0);
    state->lineBuffer.assign(kDefaultAnimWidth * kWorkingBatchLines, 0);
}

bool Renderer::ShowSDCardImage(const char *img_path, int xmin, int ymin, int batch_lines)
{
    File bmpFile = SD->open(img_path);
    if (!bmpFile)
    {
        showResourceError(tft);
        return false;
    }

    if (bmpFile.read() != 'B' || bmpFile.read() != 'M')
    {
        showResourceError(tft);
        bmpFile.close();
        return false;
    }

    bmpFile.seek(10);
    const uint32_t pixelDataOffset = bmpFile.read() |
                                     (bmpFile.read() << 8) |
                                     (bmpFile.read() << 16) |
                                     (bmpFile.read() << 24);

    bmpFile.seek(18);
    const int32_t bmpWidth = bmpFile.read() |
                             (bmpFile.read() << 8) |
                             (bmpFile.read() << 16) |
                             (bmpFile.read() << 24);
    const int32_t bmpHeight = bmpFile.read() |
                              (bmpFile.read() << 8) |
                              (bmpFile.read() << 16) |
                              (bmpFile.read() << 24);

    bmpFile.seek(28);
    const uint16_t bitsPerPixel = bmpFile.read() | (bmpFile.read() << 8);
    if (bitsPerPixel != 24)
    {
        showResourceError(tft);
        bmpFile.close();
        return false;
    }

    const int rowSize = ((bmpWidth * 3 + 3) / 4) * 4;
    const size_t rowCapacity = static_cast<size_t>(rowSize) * batch_lines;
    const size_t lineCapacity = static_cast<size_t>(bmpWidth) * batch_lines;
    if (state->rowBuffer.size() < rowCapacity)
        state->rowBuffer.resize(rowCapacity);
    if (state->lineBuffer.size() < lineCapacity)
        state->lineBuffer.resize(lineCapacity);

    bmpFile.seek(pixelDataOffset);
    for (int y = 0; y < bmpHeight; y += batch_lines)
    {
        const int actualLines = (y + batch_lines > bmpHeight) ? (bmpHeight - y) : batch_lines;
        for (int i = 0; i < actualLines; i++)
        {
            const int bytesRead = bmpFile.read(&state->rowBuffer[i * rowSize], rowSize);
            if (bytesRead != rowSize)
            {
                showResourceError(tft);
                bmpFile.close();
                return false;
            }

            for (int x = 0; x < bmpWidth; x++)
            {
                const int j = x * 3;
                const uint8_t b = state->rowBuffer[i * rowSize + j];
                const uint8_t g = state->rowBuffer[i * rowSize + j + 1];
                const uint8_t r = state->rowBuffer[i * rowSize + j + 2];
                const int destX = bmpWidth - 1 - x;
                state->lineBuffer[i * bmpWidth + destX] = ((r & 0xF8) << 8) |
                                                          ((g & 0xFC) << 3) |
                                                          (b >> 3);
            }
        }

        for (int i = 0; i < actualLines; i++)
            tft->drawRGBBitmap(xmin, ymin + y + i, &state->lineBuffer[i * bmpWidth], bmpWidth, 1);
    }

    bmpFile.close();
    return true;
}

static bool showRawFrame(SdFat *sd, Adafruit_ST7735 *tft, std::vector<uint16_t> &lineBuffer, const AnimationMeta &meta, uint16_t frameIndex)
{
    char path[128];
    snprintf(path, sizeof(path), "%s/%u.raw", meta.path, static_cast<unsigned>(frameIndex));
    File frameFile = sd->open(path);
    if (!frameFile)
        return false;

    const size_t lineCapacity = static_cast<size_t>(meta.width) * kWorkingBatchLines;
    if (lineBuffer.size() < lineCapacity)
        lineBuffer.resize(lineCapacity);

    for (uint16_t y = 0; y < meta.height; y += kWorkingBatchLines)
    {
        const uint16_t actualLines = (y + kWorkingBatchLines > meta.height) ? (meta.height - y) : kWorkingBatchLines;
        const size_t bytesNeeded = static_cast<size_t>(meta.width) * actualLines * sizeof(uint16_t);
        const int bytesRead = frameFile.read(reinterpret_cast<uint8_t *>(lineBuffer.data()), bytesNeeded);
        if (bytesRead != static_cast<int>(bytesNeeded))
        {
            frameFile.close();
            return false;
        }

        for (uint16_t i = 0; i < actualLines; ++i)
            tft->drawRGBBitmap(0, 32 + y + i, &lineBuffer[i * meta.width], meta.width, 1);
    }

    frameFile.close();
    return true;
}

bool Renderer::setAnimation(AnimationId id, bool playOnce)
{
    AnimationMeta *meta = metaFor(id);
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
    AnimationMeta *meta = metaFor(state->nowAnimId);
    bool ok = false;

    if (!meta->configured || meta->frameCount == 0 || meta->path[0] == '\0')
    {
        showResourceError(tft);
    }
    else if (meta->format == AssetFormat::BmpSequence)
    {
        char path[128];
        snprintf(path, sizeof(path), "%s/%u.bmp", meta->path, static_cast<unsigned>(state->animationIndex));
        ok = ShowSDCardImage(path, 0, 32);
    }
    else
    {
        ok = showRawFrame(SD, tft, state->lineBuffer, *meta, state->animationIndex);
        if (!ok)
            showResourceError(tft);
    }

    if (ok)
        updateRenderStats(state->stats, SD, micros() - frameStartUs);

    state->animationIndex++;
    return !ok;
}

unsigned long Renderer::frameIntervalFor(AnimationId id, unsigned long defaultIntervalMs) const
{
    const AnimationMeta *meta = metaFor(id);
    if (id == AnimationId::None || !meta->configured || meta->fpsHint == 0)
        return defaultIntervalMs;

    const unsigned long intervalMs = 1000UL / static_cast<unsigned long>(meta->fpsHint);
    return (intervalMs == 0) ? 1UL : intervalMs;
}
