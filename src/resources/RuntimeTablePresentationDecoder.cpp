#include "resources/detail/RuntimeTableDecoders.h"

#include <string.h>
#include "pet/RuntimeValueResolver.h"

namespace RuntimeTableInternal
{
namespace
{
bool compiledFeaturesAccept(uint32_t flags)
{
#if !ENABLE_INSTRUCTION_RESPONSE
    if ((flags & kInstructionResponseFeature) != 0) return false;
#endif
#if !ENABLE_SEQUENCE_AUDIO
    if ((flags & kSequenceAudioFeature) != 0) return false;
#endif
#if !ENABLE_GUESS_GAME
    if ((flags & kGuessGameFeature) != 0)
        return false;
#endif
#if !ENABLE_COMMAND_PREDICT
    if ((flags & kPredictFeature) != 0)
        return false;
#endif
#if !ENABLE_STARTUP_ANIMATION
    if ((flags & kStartupAnimationFeature) != 0)
        return false;
#endif
#if !ENABLE_FIRST_START_ANIMATION
    if ((flags & kFirstStartAnimationFeature) != 0)
        return false;
#endif
#if !ENABLE_OUTFIT_CHOOSE_ANIMATION
    if ((flags & kOutfitChooseAnimationFeature) != 0)
        return false;
#endif
    return (flags & kFirstStartAnimationFeature) == 0 ||
           (flags & kStartupAnimationFeature) != 0;
}

// Flow and FlowRoles are export/inspector metadata. Firmware executes the
// resolved system roles, including the two shared button-layout assets.
bool decodeScreenBlocks(const RuntimeTable &table, PetBehaviorConfig &config)
{
    const Section *blocks = table.find(ScreenBlocks);
    const Section *rules = table.find(ScreenRules);
    if (blocks == nullptr || blocks->count > kMaxScreenBlocks ||
        rules == nullptr || rules->count > kMaxScreenRules)
        return false;
    for (uint16_t index = 0; index < blocks->count; ++index)
    {
        uint8_t record[16];
        if (!readRecord(table.source, *blocks, index, record))
            return false;
        const uint16_t first = readU16(record + 6);
        const uint16_t count = readU16(record + 8);
        if (count > kMaxScreenRulesPerBlock || static_cast<uint32_t>(first) + count > rules->count)
            return false;
        config.screenBlocks[index] = {static_cast<ScreenBlockKind>(record[0]), record[1],
            record[2], record[3], record[4], record[5], first,
            static_cast<uint8_t>(count), readU16(record + 10)};
    }
    // Geometry, non-overlapping ranges and product frame assignment are exporter-owned.
    for (uint16_t index = 0; index < rules->count; ++index)
    {
        uint8_t record[20];
        if (!readRecord(table.source, *rules, index, record))
            return false;
        config.screenRules[index] = {readU32(record), readU32(record + 8), readU16(record + 16)};
    }
    config.screenBlockCount = static_cast<uint8_t>(blocks->count);
    config.screenRuleCount = rules->count;
    return true;
}

} // namespace

bool decodeRuntimePresentation(const RuntimeTable &table,
                               uint8_t speciesSlot,
                               uint8_t outfitSlot,
                               PetBehaviorConfig &config)
{
    if (speciesSlot == 0 || outfitSlot == 0)
        return false;
    const Source &source = table.source;
    const uint32_t featureFlags = table.featureFlags;
    if (!compiledFeaturesAccept(featureFlags) || !decodeScreenBlocks(table, config))
        return false;

    const Section *assets = table.find(AssetRefs);
    const Section *animations = table.find(Animations);
    const Section *roles = table.find(SystemRoles);
    if (assets == nullptr || animations == nullptr || roles == nullptr)
        return false;

    const ActiveAssetScope scope = {speciesSlot, outfitSlot};
    for (uint16_t index = 0; index < roles->count; ++index)
    {
        uint8_t record[8] = {};
        AssetData::AnimationRef animation = {};
        if (!readRecord(source, *roles, index, record))
            return false;
        const uint8_t role = record[0];
        if (role == 0 || role >= kFirmwarePlaybackRoleCount)
            return false;
        if (!resolveAnimation(source, *assets, *animations, readU16(record + 2), scope,
                              animation))
        {
            // Evolution uses references on each evolution record. Start
            // belongs to the initial appearance, not later species.
            if (role == static_cast<uint8_t>(FirmwarePlaybackRole::Evolution) ||
                role == static_cast<uint8_t>(FirmwarePlaybackRole::Start))
                continue;
            return false;
        }
        config.systemAnimations[role] = animation;
        if (role == static_cast<uint8_t>(FirmwarePlaybackRole::Layout))
            config.layoutUnselected = animation;
        else if (role == static_cast<uint8_t>(FirmwarePlaybackRole::LayoutSel))
            config.layoutSelected = animation;
    }
    return config.layoutUnselected.valid() && config.layoutUnselected.shared() &&
           config.layoutSelected.valid() && config.layoutSelected.shared();
}
} // namespace RuntimeTableInternal
