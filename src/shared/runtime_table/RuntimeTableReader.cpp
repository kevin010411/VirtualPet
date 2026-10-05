#include "shared/runtime_table/detail/RuntimeTableReader.h"
#include <limits.h>
#include <string.h>

namespace RuntimeTableInternal
{
namespace
{
constexpr uint8_t kMagic[4] = {'V', 'P', 'R', 'T'};
// v9 adds bounded numeric image rules; .data button-product IDs stay separate.
// Older binaries are rejected without a compatibility parser.
constexpr uint16_t kVersion = 9;
constexpr uint16_t kHeaderSize = 64;
constexpr uint16_t kSectionEntrySize = 16;
constexpr uint32_t kMaxFileSize = 16777216UL;

bool addU32(uint32_t left, uint32_t right, uint32_t &sum)
{
    if (left > UINT32_MAX - right)
        return false;
    sum = left + right;
    return true;
}

uint16_t recordSizeFor(uint16_t type)
{
    switch (type)
    {
    case Profile: return 38;
    case Persistence: return 16;
    case AssetRefs: return 12;
    case Animations: return 8;
    case PetStats: return 12;
    case PetStates: return 16;
    case Actions: return 16;
    case ActionOutcomes: return 10;
    case ActionConditions: return 16;
    case ActionEffects: return 6;
    case Buttons: return 8;
    case GuessEffects: return 8;
    case DefaultPetState: return 4;
    case StatusSets: return 12;
    case StatusConditions: return 12;
    case Appearance: return 8;
    case Species: return 8;
    case Outfits: return 8;
    case Evolutions: return 16;
    case EvolutionConditions: return 12;
    case OutfitUnlocks: return 8;
    case OutfitUnlockConditions: return 12;
    case SystemRoles: return 8;
    case Flow: return 16;
    case FlowRoles: return 8;
    case ScreenBlocks: return 16;
    case ScreenRules: return 12;
    default: return 0;
    }
}

const Section *findSection(const Section *sections, uint16_t count, uint16_t type)
{
    for (uint16_t index = 0; index < count; ++index)
        if (sections[index].type == type)
            return &sections[index];
    return nullptr;
}

bool readEnvelope(const Source &source, Section *sections, uint16_t &sectionCount,
                  uint32_t &featureFlags, uint32_t &schemaFingerprint,
                  AssetData::BundleId &bundleId, uint32_t &fileCrc32)
{
    uint8_t header[kHeaderSize] = {};
    if (source.size < kHeaderSize || source.size > kMaxFileSize ||
        !source.readAt(source.context, 0, header, sizeof(header)) ||
        memcmp(header, kMagic, sizeof(kMagic)) != 0 ||
        readU16(header + 4) != kVersion || readU16(header + 6) != kHeaderSize ||
        readU32(header + 8) != 0x01020304UL || readU32(header + 16) != source.size ||
        readU16(header + 26) != kSectionEntrySize)
        return false;
    for (size_t index = 52; index < sizeof(header); ++index)
        if (header[index] != 0)
            return false;

    featureFlags = readU32(header + 12);
    if ((featureFlags & kPetBehaviorFeature) == 0 ||
        (featureFlags & ~kKnownFeatures) != 0)
        return false;

    bool nonzeroBundle = false;
    for (uint8_t index = 0; index < sizeof(bundleId.bytes); ++index)
    {
        bundleId.bytes[index] = header[28 + index];
        nonzeroBundle = nonzeroBundle || bundleId.bytes[index] != 0;
    }
    if (!nonzeroBundle)
        return false;
    schemaFingerprint = readU32(header + 44);
    fileCrc32 = readU32(header + 48);
    sectionCount = readU16(header + 24);
    if (sectionCount == 0 || sectionCount > kMaxSections)
        return false;
    const uint32_t directoryBytes = static_cast<uint32_t>(sectionCount) * kSectionEntrySize;
    uint32_t directoryEnd = 0;
    if (!addU32(kHeaderSize, directoryBytes, directoryEnd) || directoryEnd > source.size)
        return false;

    for (uint16_t index = 0; index < sectionCount; ++index)
    {
        uint8_t entry[kSectionEntrySize] = {};
        if (!source.readAt(source.context,
                           kHeaderSize + static_cast<uint32_t>(index) * kSectionEntrySize,
                           entry, sizeof(entry)))
            return false;
        Section &section = sections[index];
        section.type = readU16(entry);
        section.offset = readU32(entry + 4);
        section.count = readU16(entry + 8);
        section.recordSize = readU16(entry + 10);
        section.length = readU32(entry + 12);
        const uint16_t expectedRecordSize = recordSizeFor(section.type);
        const uint32_t expectedLength =
            static_cast<uint32_t>(section.count) * section.recordSize;
        uint32_t end = 0;
        if (expectedRecordSize == 0 ||
            section.recordSize != expectedRecordSize || section.length != expectedLength ||
            !addU32(section.offset, section.length, end) || end > source.size)
            return false;
    }
    return true;
}

} // namespace

const Section *RuntimeTable::find(SectionType type) const
{
    return findSection(sections, sectionCount, type);
}

bool readRecord(const Source &source, const Section &section,
                uint16_t index, uint8_t *record)
{
    if (index >= section.count)
        return false;
    const uint32_t offset = section.offset +
                            static_cast<uint32_t>(index) * section.recordSize;
    return source.readAt(source.context, offset, record, section.recordSize);
}

bool readRuntimeTable(const Source &source,
                      const AssetData::RuntimeManifest *expectedManifest,
                      RuntimeTable &table)
{
    if (expectedManifest == nullptr || expectedManifest->fileSize == 0)
        return false;
    table = {};
    table.source = source;
    uint32_t fileCrc32 = 0;
    if (!readEnvelope(source, table.sections, table.sectionCount,
                       table.featureFlags, table.schemaFingerprint, table.bundleId, fileCrc32) ||
        !AssetData::sameBundleId(table.bundleId, expectedManifest->bundleId))
        return false;
    return expectedManifest->fileSize == source.size &&
           expectedManifest->schemaFingerprint == table.schemaFingerprint &&
           expectedManifest->fileCrc32 == fileCrc32;
}

bool readRuntimeManifest(const Source &source, AssetData::RuntimeManifest &manifest)
{
    manifest = {};
    Section sections[kMaxSections] = {};
    uint16_t sectionCount = 0;
    uint32_t featureFlags = 0;
    uint32_t schemaFingerprint = 0;
    uint32_t fileCrc32 = 0;
    AssetData::BundleId bundleId = {};
    if (!readEnvelope(source, sections, sectionCount, featureFlags,
                      schemaFingerprint, bundleId, fileCrc32))
        return false;
    manifest.bundleId = bundleId;
    manifest.schemaFingerprint = schemaFingerprint;
    manifest.fileSize = source.size;
    manifest.fileCrc32 = fileCrc32;
    return true;
}

bool resolveAnimation(const Source &source,
                      const Section &assets,
                      const Section &animations,
                      uint16_t animationRef,
                      const ActiveAssetScope &scope,
                      AssetData::AnimationRef &resolved)
{
    resolved = {};
    if (animationRef == kNone16 || animationRef >= animations.count)
        return false;
    uint8_t animation[8] = {};
    if (!readRecord(source, animations, animationRef, animation))
        return false;
    const uint16_t runtimeId = readU16(animation + 2);
    const uint16_t firstAsset = readU16(animation + 4);
    const uint16_t assetCount = readU16(animation + 6);
    if (runtimeId == 0 || runtimeId > AssetData::kMaxRuntimeAnimationId ||
        static_cast<uint32_t>(firstAsset) + assetCount > assets.count)
        return false;
    for (uint16_t offset = 0; offset < assetCount; ++offset)
    {
        uint8_t asset[12] = {};
        if (!readRecord(source, assets, static_cast<uint16_t>(firstAsset + offset), asset))
            return false;
        if (asset[2] == 2 ||
            (asset[2] == 1 && asset[3] == scope.speciesSlot && asset[4] == scope.outfitSlot))
        {
            resolved.speciesSlot = asset[2] == 2 ? 0 : asset[3];
            resolved.outfitSlot = asset[2] == 2 ? 0 : asset[4];
            resolved.animationId = runtimeId;
            return resolved.valid();
        }
    }
    return false;
}

bool resolveAssetReference(const Source &source, const Section &assets,
                           uint16_t reference, AssetData::AnimationRef &resolved)
{
    resolved = {};
    if (reference >= assets.count)
        return false;
    uint8_t asset[12] = {};
    if (!readRecord(source, assets, reference, asset) || readU16(asset) != reference)
        return false;
    const uint8_t scope = asset[2];
    if (scope != 1 && scope != 2)
        return false;
    if ((scope == 2 && (asset[3] != 0 || asset[4] != 0)) ||
        (scope == 1 && (asset[3] == 0 || asset[4] == 0)))
        return false;
    resolved.speciesSlot = scope == 2 ? 0 : asset[3];
    resolved.outfitSlot = scope == 2 ? 0 : asset[4];
    resolved.animationId = readU16(asset + 6);
    return resolved.valid();
}

} // namespace RuntimeTableInternal
