#include "render/AssetManifest.h"

#include <stdlib.h>
#include <string.h>

namespace
{
constexpr uint16_t kDefaultAnimWidth = 128;
constexpr uint16_t kDefaultAnimHeight = 96;
constexpr const char *kManifestPath = "/index.txt";
constexpr const char *kAnimalToken = "{animal}";
constexpr const char *kSpeciesToken = "{species}";
constexpr const char *kOutfitToken = "{outfit}";

AnimationMeta gAnimationRegistry[kAnimationIdCount] = {};

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

bool appendText(char *dest, size_t destSize, size_t &destIndex, const char *text)
{
    if (dest == nullptr || destSize == 0 || text == nullptr)
        return false;

    const size_t len = strlen(text);
    if (destIndex + len >= destSize)
        return false;

    memcpy(dest + destIndex, text, len);
    destIndex += len;
    return true;
}

bool applyAppearanceTokens(char *dest, size_t destSize, const char *source, const char *speciesCode, const char *outfitCode)
{
    if (dest == nullptr || destSize == 0 || source == nullptr)
        return false;

    const char *speciesReplacement = (speciesCode != nullptr && speciesCode[0] != '\0') ? speciesCode : "dino";
    const char *outfitReplacement = (outfitCode != nullptr && outfitCode[0] != '\0') ? outfitCode : "base";
    const size_t tokenLen = strlen(kAnimalToken);
    const size_t speciesTokenLen = strlen(kSpeciesToken);
    const size_t outfitTokenLen = strlen(kOutfitToken);

    size_t destIndex = 0;
    const char *cursor = source;
    while (*cursor != '\0')
    {
        if (strncmp(cursor, kAnimalToken, tokenLen) == 0)
        {
            if (!appendText(dest, destSize, destIndex, speciesReplacement))
                return false;
            cursor += tokenLen;
            continue;
        }
        if (strncmp(cursor, kSpeciesToken, speciesTokenLen) == 0)
        {
            if (!appendText(dest, destSize, destIndex, speciesReplacement))
                return false;
            cursor += speciesTokenLen;
            continue;
        }
        if (strncmp(cursor, kOutfitToken, outfitTokenLen) == 0)
        {
            if (!appendText(dest, destSize, destIndex, outfitReplacement))
                return false;
            cursor += outfitTokenLen;
            continue;
        }

        if (destIndex + 1 >= destSize)
            return false;
        dest[destIndex++] = *cursor++;
    }

    dest[destIndex] = '\0';
    return true;
}
} // namespace

void AssetManifest::reset()
{
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
}

bool AssetManifest::load(SdFat *sd, const char *speciesCode, const char *outfitCode)
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
        const bool isRle = strcmp(fields[1], "rle") == 0;
        if (!isBmp && !isRle)
            continue;

        AnimationMeta parsed = {};
        parsed.format = parseAssetFormat(fields[1]);
        parsed.frameCount = static_cast<uint16_t>(strtoul(fields[2], nullptr, 10));
        parsed.width = static_cast<uint16_t>(strtoul(fields[3], nullptr, 10));
        parsed.height = static_cast<uint16_t>(strtoul(fields[4], nullptr, 10));
        parsed.fpsHint = static_cast<uint8_t>(strtoul(fields[5], nullptr, 10));
        if (!applyAppearanceTokens(parsed.path, sizeof(parsed.path), fields[6], speciesCode, outfitCode))
            continue;

        if (parsed.frameCount == 0 || parsed.width == 0 || parsed.height == 0 || parsed.path[0] == '\0')
            continue;

        parsed.configured = true;
        *metaFor(targetId) = parsed;
    }

    manifest.close();
    return true;
}

AnimationMeta *AssetManifest::metaFor(AnimationId id)
{
    const size_t index = static_cast<size_t>(id);
    if (index >= kAnimationIdCount)
        return &gAnimationRegistry[0];
    return &gAnimationRegistry[index];
}

const AnimationMeta *AssetManifest::metaFor(AnimationId id) const
{
    const size_t index = static_cast<size_t>(id);
    if (index >= kAnimationIdCount)
        return &gAnimationRegistry[0];
    return &gAnimationRegistry[index];
}
