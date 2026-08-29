#include "shared/assets/AssetRuntimeContract.h"

#include <stdio.h>
#include <string.h>
#include "shared/sd/SdTextRecordReader.h"
#include "shared/utils/CanonicalDecimal.h"

namespace
{
constexpr const char *kManifestPath = "/asset_manifest.txt";
constexpr size_t kMaxManifestBytes = 16384;

bool isLowerHex(const char *text, size_t length)
{
    if (text == nullptr || strlen(text) != length)
        return false;
    for (size_t index = 0; index < length; ++index)
    {
        const char value = text[index];
        if (!((value >= '0' && value <= '9') || (value >= 'a' && value <= 'f')))
            return false;
    }
    return true;
}

bool parseLowerHex32(const char *text, uint32_t &value)
{
    if (!isLowerHex(text, 8))
        return false;
    uint32_t parsed = 0;
    for (size_t index = 0; index < 8; ++index)
    {
        const char digit = text[index];
        const uint8_t nibble = static_cast<uint8_t>(digit <= '9' ? digit - '0' : digit - 'a' + 10);
        parsed = (parsed << 4U) | nibble;
    }
    value = parsed;
    return true;
}

class ManifestDecoder
{
public:
    explicit ManifestDecoder(SdFat *sd) : sd_(sd) {}

    bool process(const SdTextRecord &record)
    {
        if (record.fieldOverflow || record.fieldCount == 0 || finished_)
            return false;
        if (!assetDataSeen_)
            return record.fieldCount == 2 && strcmp(record.fields[0], "asset_data") == 0 &&
                   strcmp(record.fields[1], "1") == 0 && (assetDataSeen_ = true);
        if (!bundleSeen_)
            return record.fieldCount == 2 && strcmp(record.fields[0], "bundle_id") == 0 &&
                   AssetData::parseBundleId(record.fields[1], candidate_.bundleId) &&
                   (bundleSeen_ = true);
        if (!animationCountSeen_)
        {
            uint32_t count = 0;
            if (record.fieldCount != 2 || strcmp(record.fields[0], "animation_count") != 0 ||
                !CanonicalDecimal::parseUnsigned(record.fields[1], AssetData::kMaxRuntimeAnimationId, count) ||
                count == 0)
                return false;
            candidate_.animationCount = static_cast<uint8_t>(count);
            animationCountSeen_ = true;
            return true;
        }
        if (animationsSeen_ < candidate_.animationCount)
            return decodeAnimation(record);
        if (!packCountSeen_)
        {
            uint32_t count = 0;
            if (record.fieldCount != 2 || strcmp(record.fields[0], "pack_count") != 0 ||
                !CanonicalDecimal::parseUnsigned(record.fields[1], 256U, count) || count == 0)
                return false;
            expectedPackCount_ = static_cast<uint16_t>(count);
            packCountSeen_ = true;
            return true;
        }
        if (packsSeen_ < expectedPackCount_)
            return decodePack(record);
        return false;
    }

    bool complete(AssetData::RuntimeManifest &manifest)
    {
        finished_ = assetDataSeen_ && bundleSeen_ && animationCountSeen_ && packCountSeen_ &&
                    animationsSeen_ == candidate_.animationCount &&
                    packsSeen_ == expectedPackCount_ && sharedSeen_ &&
                    expectedPackCount_ == static_cast<uint16_t>(candidate_.speciesPackCount) + 1U;
        if (!finished_)
            return false;
        manifest = candidate_;
        return true;
    }

private:
    bool decodeAnimation(const SdTextRecord &record)
    {
        uint32_t animationId = 0;
        const uint16_t expected = static_cast<uint16_t>(animationsSeen_) + 1U;
        if (record.fieldCount != 3 || strcmp(record.fields[0], "animation") != 0 ||
            !CanonicalDecimal::parseUnsigned(record.fields[1], AssetData::kMaxRuntimeAnimationId, animationId) ||
            animationId != expected || record.fields[2][0] == '\0')
            return false;
        for (const char *cursor = record.fields[2]; *cursor != '\0'; ++cursor)
        {
            if (static_cast<uint8_t>(*cursor) < 0x21U || static_cast<uint8_t>(*cursor) > 0x7EU)
                return false;
        }
        candidate_.animationIds[animationId] = true;
        ++animationsSeen_;
        return true;
    }

    bool decodePack(const SdTextRecord &record)
    {
        uint32_t slot = 0;
        uint32_t size = 0;
        uint32_t declaredCrc = 0;
        if (record.fieldCount != 6 || strcmp(record.fields[0], "pack") != 0 ||
            !CanonicalDecimal::parseUnsigned(record.fields[2], UINT8_MAX, slot) ||
            !CanonicalDecimal::parseUnsigned(record.fields[4], AssetData::kMaxPackBytes, size) || size < AssetData::kHeaderSize ||
            !parseLowerHex32(record.fields[5], declaredCrc))
            return false;

        char expectedPath[32] = {};
        if (packsSeen_ == 0)
        {
            if (strcmp(record.fields[1], "shared") != 0 || slot != 0)
                return false;
            strcpy(expectedPath, "assets/shared.data");
            sharedSeen_ = true;
        }
        else
        {
            const uint32_t expectedSlot = packsSeen_;
            if (strcmp(record.fields[1], "species") != 0 || slot != expectedSlot)
                return false;
            snprintf(expectedPath, sizeof(expectedPath), "assets/species_%lu.data",
                     static_cast<unsigned long>(slot));
            candidate_.speciesPackCount = static_cast<uint8_t>(slot);
        }
        if (strcmp(record.fields[3], expectedPath) != 0)
            return false;

        char physicalPath[36] = "/";
        strncat(physicalPath, expectedPath, sizeof(physicalPath) - 2);
        File file = sd_ == nullptr ? File() : sd_->open(physicalPath, FILE_READ);
        if (!file)
            return false;
        uint8_t encodedCrc[4] = {};
        const bool sizeMatches = file.size() == size;
        const bool crcReadable = file.seek(64) && file.read(encodedCrc, sizeof(encodedCrc)) == sizeof(encodedCrc);
        file.close();
        const uint32_t headerCrc = static_cast<uint32_t>(encodedCrc[0]) |
                                   (static_cast<uint32_t>(encodedCrc[1]) << 8U) |
                                   (static_cast<uint32_t>(encodedCrc[2]) << 16U) |
                                   (static_cast<uint32_t>(encodedCrc[3]) << 24U);
        if (!sizeMatches || !crcReadable || headerCrc != declaredCrc)
            return false;
        ++packsSeen_;
        return true;
    }

    SdFat *sd_;
    AssetData::RuntimeManifest candidate_ = {};
    uint16_t expectedPackCount_ = 0;
    uint16_t packsSeen_ = 0;
    uint8_t animationsSeen_ = 0;
    bool assetDataSeen_ = false;
    bool bundleSeen_ = false;
    bool animationCountSeen_ = false;
    bool packCountSeen_ = false;
    bool sharedSeen_ = false;
    bool finished_ = false;
};

bool decodeManifestRecord(void *context, const SdTextRecord &record)
{
    return context != nullptr && static_cast<ManifestDecoder *>(context)->process(record);
}
} // namespace

namespace AssetData
{
bool sameBundleId(const BundleId &left, const BundleId &right)
{
    return memcmp(left.bytes, right.bytes, sizeof(left.bytes)) == 0;
}

bool parseBundleId(const char *text, BundleId &bundleId)
{
    if (!isLowerHex(text, 32))
        return false;
    BundleId parsed = {};
    for (uint8_t index = 0; index < sizeof(parsed.bytes); ++index)
    {
        const char high = text[index * 2];
        const char low = text[index * 2 + 1];
        const uint8_t highValue = static_cast<uint8_t>(high <= '9' ? high - '0' : high - 'a' + 10);
        const uint8_t lowValue = static_cast<uint8_t>(low <= '9' ? low - '0' : low - 'a' + 10);
        parsed.bytes[index] = static_cast<uint8_t>((highValue << 4U) | lowValue);
    }
    bundleId = parsed;
    return true;
}

bool parseAnimationRef(const char *scope,
                       const char *animationId,
                       const RuntimeManifest &manifest,
                       AnimationRef &reference)
{
    uint32_t parsedId = 0;
    AnimationRef parsed = {};
    if (!CanonicalDecimal::parseUnsigned(animationId, kMaxRuntimeAnimationId, parsedId) ||
        parsedId == 0 || !manifest.animationIds[parsedId])
        return false;
    if (strcmp(scope, "shared") == 0)
    {
        parsed.animationId = static_cast<uint16_t>(parsedId);
        reference = parsed;
        return true;
    }
    if (strncmp(scope, "species:", 8) != 0)
        return false;
    const char *speciesText = scope + 8;
    const char *separator = strchr(speciesText, ':');
    if (separator == nullptr || strchr(separator + 1, ':') != nullptr)
        return false;
    char species[4] = {};
    const size_t speciesLength = static_cast<size_t>(separator - speciesText);
    if (speciesLength == 0 || speciesLength >= sizeof(species))
        return false;
    memcpy(species, speciesText, speciesLength);
    uint32_t speciesSlot = 0;
    uint32_t outfitSlot = 0;
    if (!CanonicalDecimal::parseUnsigned(species, UINT8_MAX, speciesSlot, false) ||
        !CanonicalDecimal::parseUnsigned(separator + 1, UINT8_MAX, outfitSlot, false))
        return false;
    parsed.speciesSlot = static_cast<uint8_t>(speciesSlot);
    parsed.outfitSlot = static_cast<uint8_t>(outfitSlot);
    parsed.animationId = static_cast<uint16_t>(parsedId);
    reference = parsed;
    return true;
}

bool animationReferenceExists(BundleReader &reader,
                              const AnimationRef &reference,
                              uint8_t versionIndex)
{
    if (!reference.valid() || versionIndex >= kMaxVersions)
        return false;
    AssetFrameAddress address = {};
    address.speciesSlot = reference.speciesSlot;
    address.outfitSlot = reference.outfitSlot;
    address.animationId = reference.animationId;
    address.versionIndex = versionIndex;
    AnimationRecord animation = {};
    return reader.resolveAnimation(address, animation);
}

bool loadRuntimeManifest(SdFat *sd, RuntimeManifest &manifest)
{
    manifest = {};
    ManifestDecoder decoder(sd);
    return loadSdTextRecords(sd, kManifestPath, kMaxManifestBytes,
                             "asset_manifest", "1", decodeManifestRecord, &decoder) &&
           decoder.complete(manifest);
}
} // namespace AssetData
