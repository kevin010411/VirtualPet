#include "shared/assets/AssetManifest.h"

#include <stdlib.h>
#include <string.h>

namespace
{
constexpr uint16_t kDefaultAnimWidth = 128;
constexpr uint16_t kDefaultAnimHeight = 96;
constexpr const char *kMainManifestPath = "/index/main.txt";

constexpr uint8_t kMaxLoadedAnimations = 48;

struct AnimationRegistryEntry
{
    AnimationId id = AnimationId::None;
    AnimationMeta meta = {};
};

AnimationRegistryEntry gAnimationRegistry[kMaxLoadedAnimations] = {};
uint8_t gAnimationRegistryCount = 0;
AnimationMeta gEmptyAnimationMeta = {};
char gNamedAnimationNames[AssetManifest::kMaxNamedAnimations][AssetManifest::kMaxAnimationNameLength + 1] = {};
AnimationMeta gNamedAnimationRegistry[AssetManifest::kMaxNamedAnimations] = {};
bool gPathError = false;
bool gCapacityError = false;

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

bool copyManifestPath(char *dest, size_t destSize, const char *source)
{
    if (dest == nullptr || destSize == 0 || source == nullptr)
        return false;

    const size_t len = strlen(source);
    if (len >= destSize)
        return false;

    memcpy(dest, source, len + 1);
    return true;
}

bool copyAnimationName(char *dest, size_t destSize, const char *source)
{
    if (dest == nullptr || destSize == 0 || source == nullptr || source[0] == '\0')
        return false;

    const size_t len = strlen(source);
    if (len >= destSize)
        return false;

    for (size_t i = 0; i < len; ++i)
    {
        const char c = source[i];
        const bool valid = (c >= 'A' && c <= 'Z') ||
                           (c >= 'a' && c <= 'z') ||
                           (c >= '0' && c <= '9') ||
                           c == '_' || c == '-';
        if (!valid)
            return false;
    }

    memcpy(dest, source, len + 1);
    return true;
}

bool buildAppearanceManifestPath(char *dest, size_t destSize, const char *speciesCode, const char *outfitCode)
{
    if (dest == nullptr || destSize == 0)
        return false;

    const char *species = (speciesCode != nullptr && speciesCode[0] != '\0') ? speciesCode : "dino";
    const char *outfit = (outfitCode != nullptr && outfitCode[0] != '\0') ? outfitCode : "base";
    const int written = snprintf(dest, destSize, "/index/%s_%s.txt", species, outfit);
    return written >= 0 && written < static_cast<int>(destSize);
}

void resetMeta(AnimationMeta &meta)
{
    meta.path[0] = '\0';
    meta.width = kDefaultAnimWidth;
    meta.height = kDefaultAnimHeight;
    meta.frameCount = 0;
    meta.frameIntervalMs = 0;
    meta.configured = false;
    meta.singleFile = false;
}

AnimationMeta *findMeta(AnimationId id)
{
    for (uint8_t i = 0; i < gAnimationRegistryCount; ++i)
    {
        if (gAnimationRegistry[i].id == id)
            return &gAnimationRegistry[i].meta;
    }
    return nullptr;
}

const AnimationMeta *findMetaConst(AnimationId id)
{
    for (uint8_t i = 0; i < gAnimationRegistryCount; ++i)
    {
        if (gAnimationRegistry[i].id == id)
            return &gAnimationRegistry[i].meta;
    }
    return nullptr;
}

bool upsertMeta(AnimationId id, const AnimationMeta &meta)
{
    if (id == AnimationId::None)
        return false;

    AnimationMeta *existing = findMeta(id);
    if (existing != nullptr)
    {
        *existing = meta;
        return true;
    }

    if (gAnimationRegistryCount >= kMaxLoadedAnimations)
    {
        gCapacityError = true;
        return false;
    }

    AnimationRegistryEntry &entry = gAnimationRegistry[gAnimationRegistryCount++];
    entry.id = id;
    entry.meta = meta;
    return true;
}

bool loadManifestFile(SdFat *sd, AssetManifest &registry, const char *manifestPath)
{
    if (sd == nullptr || manifestPath == nullptr || manifestPath[0] == '\0')
        return false;

    File manifest = sd->open(manifestPath, FILE_READ);
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

        if (strcmp(fields[1], "rle") != 0)
            continue;

        AnimationMeta parsed = {};
        parsed.frameIntervalMs = static_cast<uint16_t>(strtoul(fields[2], nullptr, 10));
        parsed.frameCount = static_cast<uint16_t>(strtoul(fields[3], nullptr, 10));
        parsed.width = static_cast<uint16_t>(strtoul(fields[4], nullptr, 10));
        parsed.height = static_cast<uint16_t>(strtoul(fields[5], nullptr, 10));
        if (!copyManifestPath(parsed.path, sizeof(parsed.path), fields[6]))
        {
            gPathError = true;
            continue;
        }

        if (parsed.frameCount == 0 || parsed.width == 0 || parsed.height == 0 || parsed.path[0] == '\0')
            continue;

        parsed.singleFile = endsWithIgnoreCase(parsed.path, ".rle");
        if (parsed.singleFile)
            parsed.frameCount = 1;

        parsed.configured = true;

        const AnimationId targetId = animationIdFromName(fields[0]);
        if (targetId != AnimationId::None)
        {
            upsertMeta(targetId, parsed);
        }
        else
        {
            AnimationMeta *namedMeta = registry.metaForName(fields[0]);
            if (namedMeta != nullptr)
                *namedMeta = parsed;
        }
    }

    manifest.close();
    return true;
}
} // namespace

void AssetManifest::reset()
{
    gPathError = false;
    gCapacityError = false;
    gAnimationRegistryCount = 0;
    resetMeta(gEmptyAnimationMeta);

    for (size_t i = 0; i < kMaxNamedAnimations; ++i)
    {
        gNamedAnimationNames[i][0] = '\0';
        AnimationMeta &meta = gNamedAnimationRegistry[i];
        resetMeta(meta);
    }
}

bool AssetManifest::load(SdFat *sd, const char *speciesCode, const char *outfitCode)
{
    reset();

    if (sd == nullptr)
        return false;

    char appearanceManifestPath[48];
    if (!buildAppearanceManifestPath(appearanceManifestPath, sizeof(appearanceManifestPath), speciesCode, outfitCode))
        return false;

    if (!loadManifestFile(sd, *this, kMainManifestPath))
        return false;

    return loadManifestFile(sd, *this, appearanceManifestPath);
}

bool AssetManifest::hasPathError() const
{
    return gPathError;
}

bool AssetManifest::hasCapacityError() const
{
    return gCapacityError;
}

const AnimationMeta *AssetManifest::metaFor(AnimationId id) const
{
    const AnimationMeta *meta = findMetaConst(id);
    return meta != nullptr ? meta : &gEmptyAnimationMeta;
}

AnimationMeta *AssetManifest::metaForName(const char *name)
{
    if (name == nullptr || name[0] == '\0')
        return nullptr;

    for (size_t i = 0; i < kMaxNamedAnimations; ++i)
    {
        if (strcmp(gNamedAnimationNames[i], name) == 0)
            return &gNamedAnimationRegistry[i];
    }

    for (size_t i = 0; i < kMaxNamedAnimations; ++i)
    {
        if (gNamedAnimationNames[i][0] != '\0')
            continue;

        if (!copyAnimationName(gNamedAnimationNames[i], sizeof(gNamedAnimationNames[i]), name))
            return nullptr;
        return &gNamedAnimationRegistry[i];
    }

    return nullptr;
}

const AnimationMeta *AssetManifest::metaForName(const char *name) const
{
    if (name == nullptr || name[0] == '\0')
        return nullptr;

    for (size_t i = 0; i < kMaxNamedAnimations; ++i)
    {
        if (strcmp(gNamedAnimationNames[i], name) == 0)
            return &gNamedAnimationRegistry[i];
    }

    return nullptr;
}
