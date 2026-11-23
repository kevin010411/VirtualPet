#pragma once

#include <stddef.h>
#include <stdint.h>
#include "resources/AssetRuntimeContract.h"

// Internal wire interface. Application code uses the behavior/appearance loaders.
// Source and its context must outlive the table; Sections come from a successful
// readRuntimeTable, which validates their byte ranges and record sizes.
namespace RuntimeTableInternal
{
constexpr uint16_t kMaxSections = 64;
constexpr uint16_t kNone16 = 0xffffU;
constexpr uint32_t kPetBehaviorFeature = 1UL << 0;
constexpr uint32_t kStatusFeature = 1UL << 1;
constexpr uint32_t kGuessGameFeature = 1UL << 4;
constexpr uint32_t kPredictFeature = 1UL << 5;
constexpr uint32_t kStartupAnimationFeature = 1UL << 6;
constexpr uint32_t kFirstStartAnimationFeature = 1UL << 7;
constexpr uint32_t kSequentialStatusFeature = 1UL << 9;
constexpr uint32_t kOutfitChooseAnimationFeature = 1UL << 11;
constexpr uint32_t kEvolutionPlaybackCountsFeature = 1UL << 12;
constexpr uint32_t kCollectionFeature = 1UL << 13;
constexpr uint32_t kSequenceAudioFeature = 1UL << 14;
constexpr uint32_t kInstructionResponseFeature = 1UL << 15;
constexpr uint32_t kShopFeature = 1UL << 16;
constexpr uint32_t kScreenStatSumFeature = 1UL << 17;
constexpr uint32_t kScreenStatDimensionsFeature = 1UL << 18;
constexpr uint32_t kScreenStatSumSegmentsFeature = 1UL << 19;
constexpr uint32_t kStatEventsFeature = 1UL << 20;
constexpr uint32_t kStageDaysEffectsFeature = 1UL << 22;
constexpr uint32_t kShopStageDaysFeature = 1UL << 21;
constexpr uint32_t kPersistentStatEventsFeature = 1UL << 23;
constexpr uint32_t kCollectionGameEventsFeature = 1UL << 24;
constexpr uint32_t kCaptureGameV2Feature = 1UL << 25;
constexpr uint32_t kActionOutcomeEvolutionFeature = 1UL << 26;
constexpr uint32_t kSpeciesButtonBindingsFeature = 1UL << 27;
constexpr uint32_t kCollectionReplayFeature = 1UL << 28;
constexpr uint32_t kActionEffectsAfterPlaybackFeature = 1UL << 29;
constexpr uint32_t kActionSpeciesConditionsFeature = 1UL << 30;
constexpr uint32_t kTrainingCoursesFeature = 1UL << 31;
constexpr uint32_t kKnownFeatures = (((1UL << 17) - 1UL) & ~(1UL << 8)) |
    kScreenStatSumFeature | kScreenStatDimensionsFeature | kScreenStatSumSegmentsFeature |
    kStatEventsFeature | kShopStageDaysFeature | kStageDaysEffectsFeature | kPersistentStatEventsFeature |
    kCollectionGameEventsFeature | kCaptureGameV2Feature | kActionOutcomeEvolutionFeature |
    kSpeciesButtonBindingsFeature | kCollectionReplayFeature | kActionEffectsAfterPlaybackFeature |
    kActionSpeciesConditionsFeature | kTrainingCoursesFeature;

enum SectionType : uint16_t
{
    InstructionResponseConfig = 70,
    InstructionResponseEffects = 71,
    DayCycle = 72,
    DayResetEffects = 73,
    ActionGates = 74,
    MenuDefinitions = 75,
    MenuEntries = 76,
    EventTriggers = 77,
    EventNodes = 78,
    EventEdges = 79,
    EventConditions = 80,
    EventEffects = 81,
    EventPages = 82,
    CollectionGameConfig = 83,
    CollectionEventRules = 84,
    CollectionRoundRequirements = 85,
    CollectionReplays = 86,
    TrainingCourses = 87,
    CollectionCards = 88,
    CollectionUnlockOrder = 89,
    OrderedMemoryLayouts = 90,
    CaptureArtworkSpecies = 91,
    IdleButtons = 92,
    TimingHitFrames = 93,
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
    ActionPlaybacks = 19,
    StatusSets = 20,
    StatusConditions = 21,
    DeathConfig = 22,
    AudioRefs = 23,
    ActionAudio = 24,
    IdleAudio = 25,
    AnimationAudio = 26,
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
    CollectionConfig = 60,
    CollectionItems = 61,
    CollectionConditions = 62,
    CollectionTrackedActions = 63,
};

struct Section
{
    uint32_t offset;
    uint16_t count;
    uint8_t recordSize;
    uint8_t type;
};
static_assert(sizeof(Section) == 8, "Keep the runtime directory compact");

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

struct RuntimeTableDirectory
{
    Section sections[kMaxSections];
    uint16_t sectionCount;
    uint16_t version;
    uint32_t featureFlags;
    uint32_t schemaFingerprint;
    AssetData::BundleId bundleId;

    const Section *find(SectionType type) const;
};

struct RuntimeTable : RuntimeTableDirectory
{
    Source source;
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

inline int64_t readI64(const uint8_t *bytes)
{
    const uint64_t bits = readU32(bytes) | (static_cast<uint64_t>(readU32(bytes + 4)) << 32);
    return static_cast<int64_t>(bits);
}

bool readRuntimeManifest(const Source &source, AssetData::RuntimeManifest &manifest);
bool readRuntimeTable(const Source &source,
                      const AssetData::RuntimeManifest *expectedManifest,
                      RuntimeTable &table,
                      const RuntimeTableDirectory *cached = nullptr);

// record must hold the validated section's recordSize bytes.
bool readRecord(const Source &source, const Section &section,
                uint16_t index, uint8_t *record);
bool resolveAnimation(const Source &source, const Section &assets,
                      const Section &animations, uint16_t animationRef,
                      const ActiveAssetScope &scope, AssetData::AnimationRef &resolved,
                      bool allowMissingForScope = false);
bool resolveAssetReference(const Source &source, const Section &assets,
                           uint16_t reference, AssetData::AnimationRef &resolved);
} // namespace RuntimeTableInternal
