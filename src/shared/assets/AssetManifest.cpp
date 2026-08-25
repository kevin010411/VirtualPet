#include "shared/assets/AssetManifest.h"

#include <string.h>
#include "shared/sd/SdTextRecordReader.h"
#include "shared/utils/TextBuffer.h"
#include "shared/utils/UnsignedDecimal.h"

namespace
{
constexpr uint16_t kDefaultAnimWidth = 128;
constexpr uint16_t kDefaultAnimHeight = 96;
constexpr const char *kMainManifestPath = "/index/main.txt";
constexpr size_t kMaxManifestFileBytes = 32768;

constexpr uint8_t kMaxLoadedAnimations = APP_MAX_LOADED_ANIMATIONS;

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

struct AnimationVariantEntry
{
    char baseName[AssetManifest::kMaxAnimationNameLength + 1] = {};
    char name[AssetManifest::kMaxAnimationNameLength + 1] = {};
    AnimationMeta meta = {};
};

AnimationVariantEntry gAnimationVariants[AssetManifest::kMaxAnimationVariants] = {};
uint8_t gAnimationVariantCount = 0;
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

bool splitVariantAnimationName(const char *name, char *baseName, size_t baseNameSize)
{
    if (name == nullptr || baseName == nullptr || baseNameSize == 0)
        return false;

    const size_t length = strlen(name);
    if (length < 2)
        return false;

    // Schema-v8 export tokens end in a digit (for example anim0), so use an
    // explicit separator before the positive version number. This keeps the
    // existing trailing-number convention available for legacy resources.
    const char *versionMarker = strstr(name, "_v");
    if (versionMarker != nullptr && versionMarker != name)
    {
        const char *digits = versionMarker + 2;
        if (*digits == '\0' || *digits == '0')
            return false;
        for (const char *cursor = digits; *cursor != '\0'; ++cursor)
        {
            if (*cursor < '0' || *cursor > '9')
                return false;
        }
        const size_t baseLength = static_cast<size_t>(versionMarker - name);
        if (baseLength >= baseNameSize)
            return false;
        memcpy(baseName, name, baseLength);
        baseName[baseLength] = '\0';
        return true;
    }

    size_t suffixStart = length;
    while (suffixStart > 0 && name[suffixStart - 1] >= '0' && name[suffixStart - 1] <= '9')
        --suffixStart;

    // A version suffix is a positive decimal number, so names like Dance0 and 1Dance stay named animations.
    if (suffixStart == 0 || suffixStart == length || name[suffixStart] == '0' || suffixStart >= baseNameSize)
        return false;

    memcpy(baseName, name, suffixStart);
    baseName[suffixStart] = '\0';
    return true;
}

bool isStatusSetAnimationName(const char *name)
{
    // Status Set animation IDs (for example StatusCustom1) are named
    // animations. Their numeric suffix is a Stat slot, not a legacy variant
    // version, so they must bypass splitVariantAnimationName().
    return name != nullptr && strncmp(name, "Status", 6) == 0;
}

bool buildAppearanceManifestPath(char *dest, size_t destSize, const char *speciesCode, const char *outfitCode)
{
    if (dest == nullptr || destSize == 0)
        return false;

    const char *species = (speciesCode != nullptr && speciesCode[0] != '\0') ? speciesCode : "dino";
    const char *outfit = (outfitCode != nullptr && outfitCode[0] != '\0') ? outfitCode : "base";
    TextBuffer path(dest, destSize);
    return path.append("/index/") && path.append(species) && path.append("_") &&
           path.append(outfit) && path.append(".txt") && path.ok();
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

struct ManifestLoadContext
{
    AssetManifest *registry;
    bool allowVariants;
};

SdTextRecordAction loadManifestRecord(void *rawContext, const SdTextRecord &record)
{
    if (rawContext == nullptr || record.fieldCount == 0)
        return SdTextRecordAction::Continue;
    ManifestLoadContext &context = *static_cast<ManifestLoadContext *>(rawContext);
    char *fields[7] = {};
    for (uint8_t index = 0; index < record.fieldCount && index < 7; ++index)
        fields[index] = record.fields[index];
    trimWhitespace(fields[0]);
    if (fields[0][0] == '\0' || fields[0][0] == '#')
        return SdTextRecordAction::Continue;
    if (record.fieldOverflow || record.fieldCount != 7)
        return SdTextRecordAction::Continue;
    for (size_t index = 0; index < 7; ++index)
    {
        trimWhitespace(fields[index]);
        if (fields[index][0] == '\0')
            return SdTextRecordAction::Continue;
    }

    if (strcmp(fields[1], "rle") != 0)
        return SdTextRecordAction::Continue;

    AnimationMeta parsed = {};
    parsed.frameIntervalMs = static_cast<uint16_t>(parseUnsignedDecimalUnchecked(fields[2]));
    parsed.frameCount = static_cast<uint16_t>(parseUnsignedDecimalUnchecked(fields[3]));
    parsed.width = static_cast<uint16_t>(parseUnsignedDecimalUnchecked(fields[4]));
    parsed.height = static_cast<uint16_t>(parseUnsignedDecimalUnchecked(fields[5]));
    if (!copyManifestPath(parsed.path, sizeof(parsed.path), fields[6]))
    {
        gPathError = true;
        return SdTextRecordAction::Continue;
    }

    if (parsed.frameCount == 0 || parsed.width == 0 || parsed.height == 0 || parsed.path[0] == '\0')
        return SdTextRecordAction::Continue;

    parsed.singleFile = endsWithIgnoreCase(parsed.path, ".rle");
    if (parsed.singleFile)
        parsed.frameCount = 1;

    parsed.configured = true;

    const AnimationId targetId = animationIdFromName(fields[0]);
    if (targetId != AnimationId::None)
        upsertMeta(targetId, parsed);
    else
    {
        char baseName[AssetManifest::kMaxAnimationNameLength + 1] = {};
        if (context.allowVariants && !isStatusSetAnimationName(fields[0]) &&
            splitVariantAnimationName(fields[0], baseName, sizeof(baseName)))
            context.registry->registerVariantAnimation(baseName, fields[0], parsed);
        else
            context.registry->registerNamedAnimation(fields[0], parsed);
    }
    return SdTextRecordAction::Continue;
}

bool loadManifestFile(SdFat *sd, AssetManifest &registry, const char *manifestPath, bool allowVariants)
{
    if (sd == nullptr || manifestPath == nullptr || manifestPath[0] == '\0')
        return false;
    ManifestLoadContext context = {&registry, allowVariants};
    return loadSdDelimitedTextRecords(sd, manifestPath, kMaxManifestFileBytes,
                                      kSdDelimitedTextMaxLineBytes, loadManifestRecord, &context);
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

    gAnimationVariantCount = 0;
    for (size_t i = 0; i < kMaxAnimationVariants; ++i)
    {
        gAnimationVariants[i].baseName[0] = '\0';
        gAnimationVariants[i].name[0] = '\0';
        resetMeta(gAnimationVariants[i].meta);
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

    if (!loadManifestFile(sd, *this, kMainManifestPath, false))
        return false;

    return loadManifestFile(sd, *this, appearanceManifestPath, true);
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
    for (uint8_t i = 0; i < gAnimationVariantCount; ++i)
    {
        if (strcmp(gAnimationVariants[i].name, name) == 0)
            return &gAnimationVariants[i].meta;
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
    for (uint8_t i = 0; i < gAnimationVariantCount; ++i)
    {
        if (strcmp(gAnimationVariants[i].name, name) == 0)
            return &gAnimationVariants[i].meta;
    }

    return nullptr;
}

uint8_t AssetManifest::variantCountFor(const char *baseName) const
{
    if (baseName == nullptr || baseName[0] == '\0')
        return 0;

    uint8_t count = 0;
    for (uint8_t i = 0; i < gAnimationVariantCount; ++i)
    {
        if (strcmp(gAnimationVariants[i].baseName, baseName) == 0)
            ++count;
    }
    return count;
}

const char *AssetManifest::variantNameFor(const char *baseName, uint8_t index) const
{
    if (baseName == nullptr || baseName[0] == '\0')
        return nullptr;

    uint8_t matched = 0;
    for (uint8_t i = 0; i < gAnimationVariantCount; ++i)
    {
        if (strcmp(gAnimationVariants[i].baseName, baseName) != 0)
            continue;
        if (matched == index)
            return gAnimationVariants[i].name;
        ++matched;
    }
    return nullptr;
}

bool AssetManifest::registerNamedAnimation(const char *name, const AnimationMeta &meta)
{
    if (name == nullptr || name[0] == '\0')
        return false;

    for (size_t i = 0; i < kMaxNamedAnimations; ++i)
    {
        if (strcmp(gNamedAnimationNames[i], name) == 0)
        {
            gNamedAnimationRegistry[i] = meta;
            return true;
        }
    }

    for (size_t i = 0; i < kMaxNamedAnimations; ++i)
    {
        if (gNamedAnimationNames[i][0] != '\0')
            continue;
        if (!copyAnimationName(gNamedAnimationNames[i], sizeof(gNamedAnimationNames[i]), name))
            return false;
        gNamedAnimationRegistry[i] = meta;
        return true;
    }

    gCapacityError = true;
    return false;
}

bool AssetManifest::registerVariantAnimation(const char *baseName, const char *name, const AnimationMeta &meta)
{
    if (baseName == nullptr || name == nullptr || baseName[0] == '\0' || name[0] == '\0')
        return false;

    for (uint8_t i = 0; i < gAnimationVariantCount; ++i)
    {
        if (strcmp(gAnimationVariants[i].name, name) == 0)
        {
            gAnimationVariants[i].meta = meta;
            return true;
        }
    }

    if (gAnimationVariantCount >= kMaxAnimationVariants)
    {
        gCapacityError = true;
        return false;
    }

    AnimationVariantEntry &entry = gAnimationVariants[gAnimationVariantCount];
    if (!copyAnimationName(entry.baseName, sizeof(entry.baseName), baseName) ||
        !copyAnimationName(entry.name, sizeof(entry.name), name))
    {
        return false;
    }
    entry.meta = meta;
    ++gAnimationVariantCount;
    return true;
}
