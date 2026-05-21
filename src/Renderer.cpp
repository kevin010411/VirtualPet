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
        RawRgb565Sequence,
        RleRgb565Sequence
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
    constexpr const char *kAnimalToken = "{animal}";

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
        if (strcmp(text, "rle") == 0)
            return AssetFormat::RleRgb565Sequence;
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

    bool applyAnimalToken(char *dest, size_t destSize, const char *source, const char *animalName)
    {
        if (dest == nullptr || destSize == 0 || source == nullptr)
            return false;

        const char *replacement = (animalName != nullptr) ? animalName : "";
        const size_t tokenLen = strlen(kAnimalToken);
        const size_t replacementLen = strlen(replacement);

        size_t destIndex = 0;
        const char *cursor = source;
        while (*cursor != '\0')
        {
            if (strncmp(cursor, kAnimalToken, tokenLen) == 0)
            {
                if (destIndex + replacementLen >= destSize)
                    return false;
                memcpy(dest + destIndex, replacement, replacementLen);
                destIndex += replacementLen;
                cursor += tokenLen;
                continue;
            }

            if (destIndex + 1 >= destSize)
                return false;
            dest[destIndex++] = *cursor++;
        }

        dest[destIndex] = '\0';
        return true;
    }

    bool loadManifest(SdFat *sd, const char *animalName)
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
            const bool isRle = strcmp(fields[1], "rle") == 0;
            if (!isBmp && !isRaw && !isRle)
                continue;

            AnimationMeta parsed = {};
            parsed.format = parseAssetFormat(fields[1]);
            parsed.frameCount = static_cast<uint16_t>(strtoul(fields[2], nullptr, 10));
            parsed.width = static_cast<uint16_t>(strtoul(fields[3], nullptr, 10));
            parsed.height = static_cast<uint16_t>(strtoul(fields[4], nullptr, 10));
            parsed.fpsHint = static_cast<uint8_t>(strtoul(fields[5], nullptr, 10));
            if (!applyAnimalToken(parsed.path, sizeof(parsed.path), fields[6], animalName))
                continue;

            if (parsed.frameCount == 0 || parsed.width == 0 || parsed.height == 0 || parsed.path[0] == '\0')
                continue;

            parsed.configured = true;
            *metaFor(targetId) = parsed;
        }

        manifest.close();
        return true;
    }

    bool endsWithIgnoreCase(const char *text, const char *suffix)
    {
        if (text == nullptr || suffix == nullptr)
            return false;

        const size_t textLen = strlen(text);
        const size_t suffixLen = strlen(suffix);
        if (textLen < suffixLen)
            return false;

        const char *start = text + textLen - suffixLen;
        for (size_t i = 0; i < suffixLen; ++i)
        {
            char a = start[i];
            char b = suffix[i];
            if (a >= 'A' && a <= 'Z')
                a = static_cast<char>(a - 'A' + 'a');
            if (b >= 'A' && b <= 'Z')
                b = static_cast<char>(b - 'A' + 'a');
            if (a != b)
                return false;
        }
        return true;
    }

    bool replaceOrAppendExtension(char *dest, size_t destSize, const char *path, const char *ext)
    {
        if (dest == nullptr || destSize == 0 || path == nullptr || ext == nullptr)
            return false;

        const char *dot = strrchr(path, '.');
        const char *slash = strrchr(path, '/');
        const bool hasExt = (dot != nullptr) && (slash == nullptr || dot > slash);

        if (hasExt)
            return snprintf(dest, destSize, "%.*s%s", static_cast<int>(dot - path), path, ext) < static_cast<int>(destSize);

        return snprintf(dest, destSize, "%s%s", path, ext) < static_cast<int>(destSize);
    }

    bool buildFramePath(char *dest, size_t destSize, const char *basePath, uint16_t frameIndex, const char *ext)
    {
        return snprintf(dest, destSize, "%s/%u%s", basePath, static_cast<unsigned>(frameIndex), ext) < static_cast<int>(destSize);
    }

    bool fileExists(SdFat *sd, const char *path)
    {
        if (sd == nullptr || path == nullptr)
            return false;
        return sd->exists(path);
    }
} // namespace

struct Renderer::AnimationState
{
    AnimationId nowAnimId = AnimationId::None;
    uint16_t animationIndex = 0;
    uint16_t maxFrame = 0;
    bool playOnce = false;
    Renderer::AssetFormatPreference formatPreference = Renderer::AssetFormatPreference::Auto;
    char animalName[24] = "dino";
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

static bool showBmpImage(SdFat *sd,
                         Adafruit_ST7735 *tft,
                         std::vector<uint8_t> &rowBuffer,
                         std::vector<uint16_t> &lineBuffer,
                         const char *imgPath,
                         int xmin,
                         int ymin,
                         int batchLines)
{
    File bmpFile = sd->open(imgPath);
    if (!bmpFile)
        return false;

    if (bmpFile.read() != 'B' || bmpFile.read() != 'M')
    {
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
    if (bitsPerPixel != 24 || bmpWidth <= 0 || bmpHeight <= 0)
    {
        bmpFile.close();
        return false;
    }

    const int rowSize = ((bmpWidth * 3 + 3) / 4) * 4;
    const size_t rowCapacity = static_cast<size_t>(rowSize) * batchLines;
    const size_t lineCapacity = static_cast<size_t>(bmpWidth) * batchLines;
    if (rowBuffer.size() < rowCapacity)
        rowBuffer.resize(rowCapacity);
    if (lineBuffer.size() < lineCapacity)
        lineBuffer.resize(lineCapacity);

    bmpFile.seek(pixelDataOffset);
    for (int y = 0; y < bmpHeight; y += batchLines)
    {
        const int actualLines = (y + batchLines > bmpHeight) ? (bmpHeight - y) : batchLines;
        for (int i = 0; i < actualLines; i++)
        {
            const int bytesRead = bmpFile.read(&rowBuffer[i * rowSize], rowSize);
            if (bytesRead != rowSize)
            {
                bmpFile.close();
                return false;
            }

            for (int x = 0; x < bmpWidth; x++)
            {
                const int j = x * 3;
                const uint8_t b = rowBuffer[i * rowSize + j];
                const uint8_t g = rowBuffer[i * rowSize + j + 1];
                const uint8_t r = rowBuffer[i * rowSize + j + 2];
                const int destX = bmpWidth - 1 - x;
                lineBuffer[i * bmpWidth + destX] = ((r & 0xF8) << 8) |
                                                   ((g & 0xFC) << 3) |
                                                   (b >> 3);
            }
        }

        for (int i = 0; i < actualLines; i++)
            tft->drawRGBBitmap(xmin, ymin + y + i, &lineBuffer[i * bmpWidth], bmpWidth, 1);
    }

    bmpFile.close();
    return true;
}

static bool showRawImage(SdFat *sd, Adafruit_ST7735 *tft, std::vector<uint16_t> &lineBuffer, const char *imgPath, uint16_t width, uint16_t height, int xmin, int ymin, int batchLines)
{
    File frameFile = sd->open(imgPath);
    if (!frameFile)
        return false;

    const size_t lineCapacity = static_cast<size_t>(width) * batchLines;
    if (lineBuffer.size() < lineCapacity)
        lineBuffer.resize(lineCapacity);

    for (uint16_t y = 0; y < height; y += batchLines)
    {
        const uint16_t actualLines = (y + batchLines > height) ? (height - y) : static_cast<uint16_t>(batchLines);
        const size_t bytesNeeded = static_cast<size_t>(width) * actualLines * sizeof(uint16_t);
        const int bytesRead = frameFile.read(reinterpret_cast<uint8_t *>(lineBuffer.data()), bytesNeeded);
        if (bytesRead != static_cast<int>(bytesNeeded))
        {
            frameFile.close();
            return false;
        }

        for (uint16_t i = 0; i < actualLines; ++i)
            tft->drawRGBBitmap(xmin, ymin + y + i, &lineBuffer[i * width], width, 1);
    }

    frameFile.close();
    return true;
}

static bool showRleImage(SdFat *sd,
                         Adafruit_ST7735 *tft,
                         std::vector<uint16_t> &lineBuffer,
                         const char *imgPath,
                         uint16_t expectedWidth,
                         uint16_t expectedHeight,
                         bool validateExpectedSize,
                         int xmin,
                         int ymin)
{
    File frameFile = sd->open(imgPath);
    if (!frameFile)
        return false;

    const int widthLo = frameFile.read();
    const int widthHi = frameFile.read();
    const int heightLo = frameFile.read();
    const int heightHi = frameFile.read();
    if (widthLo < 0 || widthHi < 0 || heightLo < 0 || heightHi < 0)
    {
        frameFile.close();
        return false;
    }

    const uint16_t width = static_cast<uint16_t>(widthLo | (widthHi << 8));
    const uint16_t height = static_cast<uint16_t>(heightLo | (heightHi << 8));
    if (width == 0 || height == 0)
    {
        frameFile.close();
        return false;
    }

    if (validateExpectedSize && (width != expectedWidth || height != expectedHeight))
    {
        frameFile.close();
        return false;
    }

    const size_t lineCapacity = static_cast<size_t>(width);
    if (lineBuffer.size() < lineCapacity)
        lineBuffer.resize(lineCapacity);

    uint16_t runLength = 0;
    uint16_t runColor = 0;
    size_t pixelsWritten = 0;
    const size_t totalPixels = static_cast<size_t>(width) * height;

    while (pixelsWritten < totalPixels)
    {
        const int rowIndex = static_cast<int>(pixelsWritten / width);
        size_t lineCount = 0;
        while (lineCount < width)
        {
            if (runLength == 0)
            {
                const int lowCount = frameFile.read();
                const int highCount = frameFile.read();
                const int lowColor = frameFile.read();
                const int highColor = frameFile.read();
                if (lowCount < 0 || highCount < 0 || lowColor < 0 || highColor < 0)
                {
                    frameFile.close();
                    return false;
                }

                runLength = static_cast<uint16_t>(lowCount | (highCount << 8));
                runColor = static_cast<uint16_t>(lowColor | (highColor << 8));
                if (runLength == 0)
                {
                    frameFile.close();
                    return false;
                }
            }

            const size_t copyCount = (runLength < (width - lineCount)) ? runLength : (width - lineCount);
            for (size_t i = 0; i < copyCount; ++i)
            {
                const size_t destX = (width - 1) - (lineCount + i);
                lineBuffer[destX] = runColor;
            }

            lineCount += copyCount;
            pixelsWritten += copyCount;
            runLength = static_cast<uint16_t>(runLength - copyCount);
        }

        const int y = static_cast<int>((height - 1) - rowIndex);
        tft->drawRGBBitmap(xmin, ymin + y, lineBuffer.data(), width, 1);
    }

    if (runLength != 0 || frameFile.available())
    {
        frameFile.close();
        return false;
    }

    frameFile.close();
    return true;
}

static bool showRawFrame(SdFat *sd, Adafruit_ST7735 *tft, std::vector<uint16_t> &lineBuffer, const AnimationMeta &meta, uint16_t frameIndex)
{
    char path[128];
    if (!buildFramePath(path, sizeof(path), meta.path, frameIndex, ".raw"))
        return false;
    return showRawImage(sd, tft, lineBuffer, path, meta.width, meta.height, 0, 32, kWorkingBatchLines);
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

    loadManifest(SD, state->animalName);

    const int rowCapacity = ((kDefaultAnimWidth * 3 + 3) / 4) * 4 * kWorkingBatchLines;
    state->rowBuffer.assign(rowCapacity, 0);
    state->lineBuffer.assign(kDefaultAnimWidth * kWorkingBatchLines, 0);
}

void Renderer::setAssetFormatPreference(AssetFormatPreference preference)
{
    state->formatPreference = preference;
}

void Renderer::setAssetAnimal(const char *animalName)
{
    if (animalName == nullptr || animalName[0] == '\0')
    {
        strncpy(state->animalName, "dino", sizeof(state->animalName) - 1);
        state->animalName[sizeof(state->animalName) - 1] = '\0';
        return;
    }

    strncpy(state->animalName, animalName, sizeof(state->animalName) - 1);
    state->animalName[sizeof(state->animalName) - 1] = '\0';
}

bool Renderer::ShowSDCardImage(const char *img_path, int xmin, int ymin, int batch_lines)
{
    if (img_path == nullptr || img_path[0] == '\0')
    {
        showResourceError(tft);
        return false;
    }

    bool ok = false;
    if (endsWithIgnoreCase(img_path, ".bmp"))
    {
        ok = showBmpImage(SD, tft, state->rowBuffer, state->lineBuffer, img_path, xmin, ymin, batch_lines);
    }
    else if (endsWithIgnoreCase(img_path, ".rle"))
    {
        ok = showRleImage(SD, tft, state->lineBuffer, img_path, 0, 0, false, xmin, ymin);
    }
    else if (endsWithIgnoreCase(img_path, ".raw"))
    {
        ok = showRawImage(SD, tft, state->lineBuffer, img_path, kDefaultAnimWidth, kDefaultAnimHeight, xmin, ymin, batch_lines);
    }
    else
    {
        char resolvedPath[128];
        if (replaceOrAppendExtension(resolvedPath, sizeof(resolvedPath), img_path, ".bmp"))
            ok = showBmpImage(SD, tft, state->rowBuffer, state->lineBuffer, resolvedPath, xmin, ymin, batch_lines);
        if (!ok && replaceOrAppendExtension(resolvedPath, sizeof(resolvedPath), img_path, ".rle"))
            ok = showRleImage(SD, tft, state->lineBuffer, resolvedPath, 0, 0, false, xmin, ymin);
    }

    if (!ok)
        showResourceError(tft);
    return ok;
}

bool Renderer::ShowSDCardFrame(const char *base_path, uint16_t frame_index, int xmin, int ymin, int batch_lines)
{
    if (base_path == nullptr || base_path[0] == '\0')
    {
        showResourceError(tft);
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

    if (buildFramePath(candidatePath, sizeof(candidatePath), base_path, frame_index, primaryExt) &&
        fileExists(SD, candidatePath))
    {
        ok = ShowSDCardImage(candidatePath, xmin, ymin, batch_lines);
    }
    else if (buildFramePath(candidatePath, sizeof(candidatePath), base_path, frame_index, fallbackExt) &&
             fileExists(SD, candidatePath))
    {
        ok = ShowSDCardImage(candidatePath, xmin, ymin, batch_lines);
    }
    else if (buildFramePath(candidatePath, sizeof(candidatePath), base_path, frame_index, primaryExt))
    {
        ok = ShowSDCardImage(candidatePath, xmin, ymin, batch_lines);
    }

    return ok;
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
    else if (meta->format == AssetFormat::RawRgb565Sequence)
    {
        ok = showRawFrame(SD, tft, state->lineBuffer, *meta, state->animationIndex);
        if (!ok)
            showResourceError(tft);
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

        if (buildFramePath(primaryPath, sizeof(primaryPath), meta->path, state->animationIndex, primaryExt))
        {
            if (endsWithIgnoreCase(primaryPath, ".rle"))
                ok = showRleImage(SD, tft, state->lineBuffer, primaryPath, meta->width, meta->height, true, 0, 32);
            else
                ok = ShowSDCardImage(primaryPath, 0, 32);
        }

        if (!ok &&
            buildFramePath(fallbackPath, sizeof(fallbackPath), meta->path, state->animationIndex, fallbackExt) &&
            fileExists(SD, fallbackPath))
        {
            if (endsWithIgnoreCase(fallbackPath, ".rle"))
                ok = showRleImage(SD, tft, state->lineBuffer, fallbackPath, meta->width, meta->height, true, 0, 32);
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
    const AnimationMeta *meta = metaFor(id);
    if (id == AnimationId::None || !meta->configured || meta->fpsHint == 0)
        return defaultIntervalMs;

    const unsigned long intervalMs = 1000UL / static_cast<unsigned long>(meta->fpsHint);
    return (intervalMs == 0) ? 1UL : intervalMs;
}
