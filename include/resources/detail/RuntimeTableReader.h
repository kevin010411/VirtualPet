#pragma once

#include <stddef.h>
#include <stdint.h>
#include "resources/AssetRuntimeContract.h"

// Internal wire interface. Application code uses the behavior/appearance loaders.
// Source and its context must outlive the table; Sections come from a successful
// readRuntimeTable, which validates their byte ranges and record sizes.
namespace RuntimeTableInternal
{
constexpr uint16_t kMaxSections = 32;
constexpr uint16_t kNone16 = 0xffffU;
constexpr uint32_t kPetBehaviorFeature = 1UL << 0;
constexpr uint32_t kStatusFeature = 1UL << 1;
constexpr uint32_t kGuessGameFeature = 1UL << 4;
constexpr uint32_t kPredictFeature = 1UL << 5;
constexpr uint32_t kStartupAnimationFeature = 1UL << 6;
constexpr uint32_t kFirstStartAnimationFeature = 1UL << 7;
constexpr uint32_t kSequentialStatusFeature = 1UL << 9;
constexpr uint32_t kOutfitChooseAnimationFeature = 1UL << 11;
constexpr uint32_t kKnownFeatures = ((1UL << 12) - 1UL) & ~(1UL << 8);

enum SectionType : uint16_t
{
    Profile = 1,
    Persistence = 2,
    AssetRefs = 3,
    Animations = 4,
    PetStats = 10,
    PetStates = 11,
    Actions = 12,
    ActionOutcomes = 13,
    ActionConditions = 14,
    ActionEffects = 15,
    Buttons = 16,
    GuessEffects = 17,
    DefaultPetState = 18,
    StatusSets = 20,
    StatusConditions = 21,
    Appearance = 30,
    Species = 31,
    Outfits = 32,
    Evolutions = 33,
    EvolutionConditions = 34,
    OutfitUnlocks = 35,
    OutfitUnlockConditions = 36,
    SystemRoles = 40,
    Flow = 42,
    FlowRoles = 43,
    ScreenBlocks = 50,
    ScreenRules = 51,
};

struct Section
{
    uint16_t type;
    uint16_t count;
    uint16_t recordSize;
    uint32_t offset;
    uint32_t length;
};

struct ActiveAssetScope
{
    uint8_t speciesSlot;
    uint8_t outfitSlot;
};

using ReadAt = bool (*)(void *, uint32_t, uint8_t *, size_t);

struct Source
{
    void *context;
    ReadAt readAt;
    uint32_t size;
};

struct RuntimeTable
{
    Source source;
    Section sections[kMaxSections];
    uint16_t sectionCount;
    uint32_t featureFlags;
    uint32_t schemaFingerprint;
    AssetData::BundleId bundleId;

    const Section *find(SectionType type) const;
};

inline uint16_t readU16(const uint8_t *bytes)
{
    return static_cast<uint16_t>(bytes[0] | (static_cast<uint16_t>(bytes[1]) << 8));
}

inline int16_t readI16(const uint8_t *bytes)
{
    return static_cast<int16_t>(readU16(bytes));
}

inline uint32_t readU32(const uint8_t *bytes)
{
    return static_cast<uint32_t>(bytes[0]) |
           (static_cast<uint32_t>(bytes[1]) << 8) |
           (static_cast<uint32_t>(bytes[2]) << 16) |
           (static_cast<uint32_t>(bytes[3]) << 24);
}

inline int32_t readI32(const uint8_t *bytes)
{
    return static_cast<int32_t>(readU32(bytes));
}

bool readRuntimeManifest(const Source &source, AssetData::RuntimeManifest &manifest);
bool readRuntimeTable(const Source &source,
                      const AssetData::RuntimeManifest *expectedManifest,
                      RuntimeTable &table);

// record must hold the validated section's recordSize bytes.
bool readRecord(const Source &source, const Section &section,
                uint16_t index, uint8_t *record);
bool resolveAnimation(const Source &source, const Section &assets,
                      const Section &animations, uint16_t animationRef,
                      const ActiveAssetScope &scope, AssetData::AnimationRef &resolved);
bool resolveAssetReference(const Source &source, const Section &assets,
                           uint16_t reference, AssetData::AnimationRef &resolved);
} // namespace RuntimeTableInternal
