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
    config.screenBlockCount = 0;
    config.screenRuleCount = 0;
    uint16_t nextRule = 0;
    uint16_t nextFrame = blocks->count + 2;
    uint8_t animationCount = 0;
    for (uint16_t index = 0; index < blocks->count; ++index)
    {
        uint8_t record[16] = {};
        if (!readRecord(table.source, *blocks, index, record) ||
            readU32(record + 12) != 0)
            return false;
        ScreenBlockConfig &block = config.screenBlocks[index];
        block = {static_cast<ScreenBlockKind>(record[0]), record[1],
                 record[2], record[3], record[4], record[5], 0, 0, readU16(record + 10)};
        const uint16_t firstRule = readU16(record + 6);
        const uint16_t ruleCount = readU16(record + 8);
        if (block.kind != ScreenBlockKind::Stat && (firstRule || ruleCount || block.fallbackFrame))
            return false;
        if (block.width == 0 || block.height == 0 ||
            static_cast<uint16_t>(block.x) + block.width > 128 ||
            static_cast<uint16_t>(block.y) + block.height > 160 ||
            (block.x | block.y | block.width | block.height) % 16 != 0)
            return false;
        if (block.kind == ScreenBlockKind::Animation)
        {
            if (block.source != 0 || ++animationCount > 1)
                return false;
        }
        else if (block.kind == ScreenBlockKind::Button)
        {
            if (block.source == 0 || block.source > kPetBehaviorButtonCount)
                return false;
        }
        else if (block.kind == ScreenBlockKind::Stat)
        {
            if (block.source != kUnboundScreenSource && block.source != kRuntimeValueStageDays &&
                (!isRuntimeValueIdPetStat(block.source) || runtimePetStatSlot(block.source) >= config.statCount))
                return false;
            if (firstRule != nextRule || ruleCount > kMaxScreenRulesPerBlock || firstRule + ruleCount > rules->count)
                return false;
            block.firstRule = firstRule;
            block.ruleCount = static_cast<uint8_t>(ruleCount);
            uint16_t localFrames[kMaxScreenRulesPerBlock + 1] = {};
            uint8_t localCount = 0;
            for (uint16_t child = 0; child <= ruleCount; ++child)
            {
                uint16_t frame = block.fallbackFrame;
                if (child != 0)
                {
                    uint8_t ruleRecord[20] = {};
                    if (!readRecord(table.source, *rules, firstRule + child - 1, ruleRecord) || readU16(ruleRecord + 18))
                        return false;
                    ScreenRuleConfig &rule = config.screenRules[firstRule + child - 1];
                    rule = {readI64(ruleRecord), readI64(ruleRecord + 8), readU16(ruleRecord + 16)};
                    if (rule.minimum > rule.maximum)
                        return false;
                    for (uint16_t earlier = firstRule; earlier < firstRule + child - 1; ++earlier)
                    {
                        const ScreenRuleConfig &other = config.screenRules[earlier];
                        if (rule.minimum <= other.maximum && rule.maximum >= other.minimum)
                            return false;
                    }
                    frame = rule.frame;
                }
                bool known = frame == index + 2;
                for (uint8_t entry = 0; entry < localCount; ++entry)
                    known = known || localFrames[entry] == frame;
                if (!known)
                {
                    if (frame != nextFrame)
                        return false;
                    localFrames[localCount++] = frame;
                    ++nextFrame;
                }
            }
            nextRule += ruleCount;
        }
        else
            return false;
        for (uint16_t previous = 0; previous < index; ++previous)
        {
            const ScreenBlockConfig &other = config.screenBlocks[previous];
            if (block.x < other.x + other.width && block.x + block.width > other.x &&
                block.y < other.y + other.height && block.y + block.height > other.y)
                return false;
        }
    }
    // Publish the count only after the entire section has passed validation.
    if (nextRule != rules->count)
        return false;
    config.screenRuleCount = rules->count;
    config.screenProductFrameCount = nextFrame - 1;
    config.screenBlockCount = static_cast<uint8_t>(blocks->count);
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

    memset(config.systemAnimations, 0, sizeof(config.systemAnimations));
    config.layoutUnselected = {};
    config.layoutSelected = {};
    const ActiveAssetScope scope = {speciesSlot, outfitSlot};
    for (uint16_t index = 0; index < roles->count; ++index)
    {
        uint8_t record[8] = {};
        AssetData::AnimationRef animation = {};
        if (!readRecord(source, *roles, index, record))
            return false;
        const uint8_t role = record[0];
        if (role == 0 || role >= kFirmwarePlaybackRoleCount ||
            readU16(record + 4) != 0)
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
