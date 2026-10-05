#include "pet_behavior/domain/RuntimeTableBehavior.h"

#include <limits.h>
#include <string.h>
#include "commands/domain/SystemCommandCatalog.h"
#include "pet_behavior/domain/RuntimeValueResolver.h"
#include "shared/runtime_table/detail/RuntimeTableFile.h"
#include "appearance/domain/detail/RuntimeTableAppearanceDecoder.h"

using namespace RuntimeTableInternal;

#ifndef ENABLE_GUESS_GAME_SINGLE_ROUND
#define ENABLE_GUESS_GAME_SINGLE_ROUND 0
#endif

namespace
{
struct MemorySource
{
    const uint8_t *bytes;
    uint32_t size;
};

bool readMemory(void *context, uint32_t offset, uint8_t *destination, size_t size)
{
    MemorySource &source = *static_cast<MemorySource *>(context);
    if (destination == nullptr || offset > source.size || size > source.size - offset)
        return false;
    memcpy(destination, source.bytes + offset, size);
    return true;
}

void clearOwnedBehavior(PetBehaviorConfig &config)
{
    memset(config.stats, 0, sizeof(config.stats));
    memset(config.petStates, 0, sizeof(config.petStates));
    memset(config.actions, 0, sizeof(config.actions));
    memset(config.actionOutcomes, 0, sizeof(config.actionOutcomes));
    memset(config.actionConditions, 0, sizeof(config.actionConditions));
    memset(config.actionEffects, 0, sizeof(config.actionEffects));
#if ENABLE_GUESS_GAME
    memset(config.guessEffects, 0, sizeof(config.guessEffects));
#endif
    memset(config.buttons, 0, sizeof(config.buttons));
    memset(&config.statusSets, 0, sizeof(config.statusSets));
    config.statCount = 0;
    config.petStateCount = 0;
    config.idleAnimation = {};
    config.actionCount = 0;
    config.actionConditionCount = 0;
    config.actionEffectCount = 0;
    config.actionOutcomeCount = 0;
#if ENABLE_GUESS_GAME
    config.guessEffectCount = 0;
#endif
    config.buttonCount = 0;
}

bool decodeStats(const Source &source, const Section &section, PetBehaviorConfig &config)
{
    if (section.count == 0 || section.count > kPetBehaviorSlotCount)
        return false;
    for (uint16_t index = 0; index < section.count; ++index)
    {
        uint8_t record[12] = {};
        if (!readRecord(source, section, index, record))
            return false;
        PetBehaviorStatConfig &stat = config.stats[index];
        stat.active = true;
        stat.initialValue = readI16(record + 2);
        stat.minValue = readI16(record + 4);
        stat.maxValue = readI16(record + 6);
        stat.dailyChange = readI16(record + 8);
    }
    config.statCount = static_cast<uint8_t>(section.count);
    return true;
}

bool decodePetStates(const Source &source,
                     const Section *states,
                     const Section *defaultState,
                     const Section &assets,
                     const Section &animations,
                     const ActiveAssetScope &scope,
                     PetBehaviorConfig &config)
{
    if (states == nullptr || defaultState == nullptr ||
        states->count > kMaxRuntimePetStates || defaultState->count != 1)
        return false;

    for (uint16_t index = 0; index < states->count; ++index)
    {
        uint8_t record[16] = {};
        AssetData::AnimationRef animation = {};
        if (!readRecord(source, *states, index, record))
            return false;
        const RuntimeRangePredicate predicate = {
            readU16(record + 2), readI32(record + 4), readI32(record + 8)};
        if (readU16(record) != index || readU16(record + 14) != 0 ||
            !isRuntimeBehaviorRange(predicate) ||
            (isRuntimeValueIdPetStat(predicate.valueId) &&
             runtimePetStatSlot(predicate.valueId) >= config.statCount) ||
            !resolveAnimation(source, assets, animations, readU16(record + 12), scope, animation))
            return false;

        RuntimePetStateConfig &state = config.petStates[index];
        state.predicate = predicate;
        state.idleAnimation = animation;
    }

    uint8_t record[4] = {};
    if (!readRecord(source, *defaultState, 0, record) ||
        readU16(record + 2) != 0 ||
        !resolveAnimation(source, assets, animations, readU16(record), scope,
                          config.idleAnimation))
        return false;

    config.petStateCount = static_cast<uint8_t>(states->count);
    return true;
}

bool decodeGuessEffects(const Source &source,
                        uint32_t featureFlags,
                        const Section *effects,
                        PetBehaviorConfig &config)
{
    if ((featureFlags & kGuessGameFeature) == 0)
        return effects == nullptr;
#if !ENABLE_GUESS_GAME
    return false;
#else
    if (effects == nullptr)
        return false;
    if (effects->count > kMaxPetBehaviorGuessEffects)
        return false;

    for (uint16_t index = 0; index < effects->count; ++index)
    {
        uint8_t record[8] = {};
        if (!readRecord(source, *effects, index, record) || record[1] >= config.statCount)
            return false;

        PetBehaviorGuessEffectConfig &effect = config.guessEffects[index];
        effect.active = true;
        effect.outcome = static_cast<PetBehaviorGuessOutcome>(record[0]);
        effect.statSlot = record[1];
        effect.operation = static_cast<PetBehaviorEffectOperation>(record[2]);
        effect.value = readI16(record + 4);
    }
    config.guessEffectCount = static_cast<uint8_t>(effects->count);
    return true;
#endif
}

bool decodeActions(const Source &source,
                   const Section &actions,
                   const Section &outcomes,
                   const Section *conditions,
                   const Section *effects,
                   const Section &assets,
                   const Section &animations,
                   const ActiveAssetScope &scope,
                   PetBehaviorConfig &config)
{
    if (actions.count > kMaxPetBehaviorActions || outcomes.count > kMaxPetBehaviorActionOutcomes ||
        (conditions != nullptr && conditions->count > kMaxPetBehaviorActionConditions) ||
        (effects != nullptr && effects->count > kMaxPetBehaviorActionEffects))
        return false;
    uint16_t nextOutcome = 0;
    uint16_t nextCondition = 0;
    uint16_t nextEffect = 0;
    for (uint16_t actionIndex = 0; actionIndex < actions.count; ++actionIndex)
    {
        uint8_t actionRecord[16] = {};
        if (!readRecord(source, actions, actionIndex, actionRecord))
            return false;
        const uint8_t mode = actionRecord[1];
        const uint8_t outcomeCount = actionRecord[6];
        const uint8_t conditionCount = actionRecord[10];
        const bool hasFallback = (actionRecord[3] & 1U) != 0;
        if (mode > static_cast<uint8_t>(PetBehaviorActionMode::RandomOutcome) ||
            (mode == 2 ? (outcomeCount < kMinPetBehaviorRandomOutcomesPerAction ||
                          outcomeCount > kMaxPetBehaviorRandomOutcomesPerAction)
                       : outcomeCount != 1) ||
            conditionCount > kMaxPetBehaviorActionConditionsPerAction ||
            static_cast<uint32_t>(nextOutcome) + outcomeCount > outcomes.count ||
            (conditionCount != 0 && conditions == nullptr) ||
            static_cast<uint32_t>(nextCondition) + conditionCount >
                (conditions == nullptr ? 0 : conditions->count))
            return false;

        PetBehaviorActionConfig &action = config.actions[actionIndex];
        action.active = true;
        action.mode = static_cast<PetBehaviorActionMode>(mode);
        action.suspendDailyChangeDays = actionRecord[2];
        action.hasFallbackAnimation = hasFallback;
        action.firstOutcome = static_cast<uint8_t>(nextOutcome);
        action.outcomeCount = outcomeCount;
        action.firstCondition = static_cast<uint8_t>(nextCondition);
        action.conditionCount = conditionCount;

        for (uint8_t outcomeSlot = 0; outcomeSlot < outcomeCount; ++outcomeSlot)
        {
            uint8_t outcomeRecord[10] = {};
            if (!readRecord(source, outcomes, nextOutcome, outcomeRecord) ||
                readU16(outcomeRecord + 4) != nextEffect ||
                static_cast<uint32_t>(nextEffect) + readU16(outcomeRecord + 6) >
                    (effects == nullptr ? 0 : effects->count) ||
                readU16(outcomeRecord + 8) != 0)
                return false;
            const uint8_t weight = outcomeRecord[0];
            const uint8_t playbackCount = outcomeRecord[1];
            const uint16_t animationRef = readU16(outcomeRecord + 2);
            const uint16_t effectCount = readU16(outcomeRecord + 6);
            if (effectCount > kPetBehaviorSlotCount)
                return false;
            AssetData::AnimationRef animation = {};
            const bool missingConditionalFallback = mode == 1 && !hasFallback;
            if (!missingConditionalFallback &&
                !resolveAnimation(source, assets, animations, animationRef, scope, animation))
                return false;

            PetBehaviorActionOutcomeConfig &outcome = config.actionOutcomes[nextOutcome];
            outcome.weight = weight;
            outcome.animationPlayback.animation = animation;
            outcome.animationPlayback.playbackCount = playbackCount;
            outcome.firstEffect = static_cast<uint8_t>(nextEffect);
            outcome.effectCount = static_cast<uint8_t>(effectCount);

            for (uint16_t effectOffset = 0; effectOffset < effectCount; ++effectOffset)
            {
                uint8_t effectRecord[6] = {};
                if (effects == nullptr || !readRecord(source, *effects, nextEffect, effectRecord) ||
                    effectRecord[0] >= config.statCount || effectRecord[1] > 1 ||
                    readU16(effectRecord + 4) != 0)
                    return false;
                PetBehaviorActionEffectConfig &effect = config.actionEffects[nextEffect];
                effect.statSlot = effectRecord[0];
                effect.operation = static_cast<PetBehaviorEffectOperation>(effectRecord[1]);
                effect.value = readI16(effectRecord + 2);
                ++nextEffect;
            }
            ++nextOutcome;
        }

        for (uint8_t conditionSlot = 0; conditionSlot < conditionCount; ++conditionSlot)
        {
            uint8_t conditionRecord[16] = {};
            if (conditions == nullptr ||
                !readRecord(source, *conditions, nextCondition, conditionRecord))
                return false;
            const RuntimeRangePredicate predicate = {
                readU16(conditionRecord + 2), readI32(conditionRecord + 8),
                readI32(conditionRecord + 12)};
            if (conditionRecord[1] != 0 || conditionRecord[5] != 0 ||
                conditionRecord[4] == 0 || conditionRecord[4] > 5 ||
                !isRuntimeBehaviorRange(predicate) ||
                (isRuntimeValueIdPetStat(predicate.valueId) &&
                 runtimePetStatSlot(predicate.valueId) >= config.statCount))
                return false;
            PetBehaviorActionConditionConfig &condition =
                config.actionConditions[nextCondition];
            condition.priority = conditionRecord[0];
            condition.predicate = predicate;
            condition.animationPlayback.playbackCount = conditionRecord[4];
            if (!resolveAnimation(source, assets, animations, readU16(conditionRecord + 6), scope,
                                  condition.animationPlayback.animation))
                return false;
            ++nextCondition;
        }
    }
    config.actionCount = static_cast<uint8_t>(actions.count);
    config.actionOutcomeCount = static_cast<uint8_t>(nextOutcome);
    config.actionConditionCount = static_cast<uint8_t>(nextCondition);
    config.actionEffectCount = static_cast<uint8_t>(nextEffect);
    return nextOutcome == outcomes.count &&
           nextCondition == (conditions == nullptr ? 0 : conditions->count) &&
           nextEffect == (effects == nullptr ? 0 : effects->count);
}

bool decodeButtons(const Source &source, const Section &buttons, PetBehaviorConfig &config)
{
    if (buttons.count != kPetBehaviorButtonCount)
        return false;
    for (uint16_t index = 0; index < buttons.count; ++index)
    {
        uint8_t record[8] = {};
        if (!readRecord(source, buttons, index, record) || record[0] != index + 1 ||
            record[1] > static_cast<uint8_t>(PetBehaviorButtonKind::SystemCommand) ||
            readU32(record + 4) != 0)
            return false;
        PetBehaviorButtonConfig &button = config.buttons[index];
        button.active = true;
        button.kind = static_cast<PetBehaviorButtonKind>(record[1]);
        const uint16_t target = readU16(record + 2);
        if (button.kind == PetBehaviorButtonKind::Empty)
        {
            if (target != kNone16)
                return false;
        }
        else if (button.kind == PetBehaviorButtonKind::UserAction)
        {
            if (target >= config.actionCount)
                return false;
            button.actionSlot = static_cast<uint8_t>(target);
        }
        else
        {
            if (findCompiledSystemCommand(static_cast<RuntimeSystemCommandId>(target)) == nullptr)
                return false;
            button.systemCommandId = static_cast<RuntimeSystemCommandId>(target);
        }
    }
    config.buttonCount = static_cast<uint8_t>(buttons.count);
    return true;
}

bool decodeStatus(const Source &source,
                  uint32_t featureFlags,
                  const Section *sets,
                  const Section *conditions,
                  const Section &assets,
                  const Section &animations,
                  const ActiveAssetScope &scope,
                  PetBehaviorConfig &config)
{
    const bool enabled = (featureFlags & kStatusFeature) != 0;
    if (!enabled)
        return sets == nullptr && conditions == nullptr;
    if (sets == nullptr || conditions == nullptr || sets->count == 0 ||
        sets->count > kMaxStatusSets || conditions->count > kMaxStatusSets * kMaxStatusConditions)
        return false;
    uint16_t nextCondition = 0;
    for (uint16_t setIndex = 0; setIndex < sets->count; ++setIndex)
    {
        uint8_t setRecord[12] = {};
        if (!readRecord(source, *sets, setIndex, setRecord) ||
            setRecord[0] != setIndex ||
            setRecord[1] > kMaxStatusConditions ||
            readU16(setRecord + 4) != nextCondition ||
            static_cast<uint32_t>(nextCondition) + setRecord[1] > conditions->count)
            return false;
        StatusSetConfig &set = config.statusSets.sets[setIndex];
        set.conditionCount = setRecord[1];
        set.versionCount = readU16(setRecord + 6);
        if (set.versionCount == 0 || set.versionCount > AssetData::kMaxVersions ||
            setRecord[8] != 1 ||
            setRecord[9] != 0 || readU16(setRecord + 10) != 0)
            return false;
        if (!resolveAnimation(source, assets, animations, readU16(setRecord + 2),
                              scope, set.animation))
            return false;
        uint16_t versionProduct = 1;
        for (uint8_t conditionIndex = 0; conditionIndex < set.conditionCount; ++conditionIndex)
        {
            uint8_t conditionRecord[12] = {};
            if (!readRecord(source, *conditions, nextCondition, conditionRecord) ||
                conditionRecord[3] == 0 || conditionRecord[3] > 32 ||
                conditionRecord[0] > 1 ||
                readI32(conditionRecord + 4) > readI32(conditionRecord + 8))
                return false;
            StatusSetCondition &condition = set.conditions[conditionIndex];
            condition.kind = static_cast<StatusConditionKind>(conditionRecord[0]);
            const uint16_t sourceValue = readU16(conditionRecord + 1);
            condition.valueId = condition.kind == StatusConditionKind::PetStatusAxis
                                    ? 0
                                    : sourceValue;
            condition.petStateMask = condition.kind == StatusConditionKind::PetStatusAxis
                                         ? sourceValue
                                         : 0;
            condition.levels = conditionRecord[3];
            condition.minValue = readI32(conditionRecord + 4);
            condition.maxValue = readI32(conditionRecord + 8);
            if (condition.kind == StatusConditionKind::PetStatusAxis)
            {
                const uint16_t allowedMask = config.petStateCount >= 16
                                                   ? 0xFFFFU
                                                   : static_cast<uint16_t>((1U << config.petStateCount) - 1U);
                const uint16_t selectedMask = condition.petStateMask;
                uint8_t selectedCount = 0;
                for (uint16_t mask = selectedMask; mask != 0; mask >>= 1)
                    selectedCount = static_cast<uint8_t>(selectedCount + (mask & 1U));
                if (selectedMask == 0 || (selectedMask & ~allowedMask) != 0 ||
                    condition.levels != static_cast<uint8_t>(selectedCount + 1))
                    return false;
            }
            else
            {
                const RuntimeRangePredicate predicate = {
                    condition.valueId, condition.minValue, condition.maxValue};
                if (!isRuntimeBehaviorRange(predicate) ||
                    (isRuntimeValueIdPetStat(condition.valueId) &&
                     runtimePetStatSlot(condition.valueId) >= config.statCount))
                    return false;
            }
            versionProduct = static_cast<uint16_t>(
                versionProduct * condition.levels);
            if (versionProduct > AssetData::kMaxVersions)
                return false;
            ++nextCondition;
        }
        if (versionProduct != set.versionCount)
            return false;
    }
    config.statusSets.count = static_cast<uint8_t>(sets->count);
    return nextCondition == conditions->count;
}

bool decodeRuntimeTableBehavior(const RuntimeTable &table,
                                const AssetData::RuntimeManifest &manifest,
                                uint8_t speciesSlot,
                                uint8_t outfitSlot,
                                PetBehaviorConfig &config)
{
    if (speciesSlot == 0 || outfitSlot == 0)
        return false;
    const Source &source = table.source;
    const Section *assets = table.find(AssetRefs);
    const Section *animations = table.find(Animations);
    const Section *stats = table.find(PetStats);
    const Section *petStates = table.find(PetStates);
    const Section *defaultPetState = table.find(DefaultPetState);
    const Section *actions = table.find(Actions);
    const Section *outcomes = table.find(ActionOutcomes);
    const Section *actionConditions = table.find(ActionConditions);
    const Section *effects = table.find(ActionEffects);
    const Section *guessEffects = table.find(GuessEffects);
    const Section *buttons = table.find(Buttons);
    if (assets == nullptr || animations == nullptr || stats == nullptr || actions == nullptr ||
        outcomes == nullptr || buttons == nullptr)
        return false;

    // The caller discards this configuration when decoding fails.  Decode
    // directly into it to keep the STM32 startup stack bounded.
    config = {};
    clearOwnedBehavior(config);
    config.assetManifest = manifest;
    config.activeSpeciesSlot = speciesSlot;
    config.activeOutfitSlot = outfitSlot;
    config.schemaFingerprint = table.schemaFingerprint;
    const ActiveAssetScope scope = {speciesSlot, outfitSlot};
    if (!decodeStats(source, *stats, config) ||
        !decodePetStates(source, petStates, defaultPetState,
                         *assets, *animations, scope, config) ||
        !decodeActions(source, *actions, *outcomes, actionConditions, effects,
                       *assets, *animations, scope, config) ||
        !decodeGuessEffects(source, table.featureFlags, guessEffects, config) ||
        !decodeButtons(source, *buttons, config) ||
        !decodeStatus(source, table.featureFlags,
                      table.find(StatusSets), table.find(StatusConditions),
                      *assets, *animations, scope, config))
        return false;
    return true;
}

bool compiledFeaturesAccept(uint32_t flags)
{
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
            if (firstRule != nextRule || ruleCount > 32 || firstRule + ruleCount > rules->count)
                return false;
            block.firstRule = static_cast<uint8_t>(firstRule);
            block.ruleCount = static_cast<uint8_t>(ruleCount);
            uint16_t localFrames[33] = {};
            uint8_t localCount = 0;
            for (uint16_t child = 0; child <= ruleCount; ++child)
            {
                uint16_t frame = block.fallbackFrame;
                if (child != 0)
                {
                    uint8_t ruleRecord[12] = {};
                    if (!readRecord(table.source, *rules, firstRule + child - 1, ruleRecord) || readU16(ruleRecord + 10))
                        return false;
                    ScreenRuleConfig &rule = config.screenRules[firstRule + child - 1];
                    rule = {readI32(ruleRecord), readI32(ruleRecord + 4), readU16(ruleRecord + 8)};
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
    config.screenRuleCount = static_cast<uint8_t>(rules->count);
    config.screenProductFrameCount = nextFrame - 1;
    config.screenBlockCount = static_cast<uint8_t>(blocks->count);
    return true;
}

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
    if (roles != nullptr)
    {
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
    }
    return config.layoutUnselected.valid() && config.layoutUnselected.shared() &&
           config.layoutSelected.valid() && config.layoutSelected.shared();
}
} // namespace

bool parseRuntimeTableBehavior(const uint8_t *bytes,
                               size_t byteCount,
                               const AssetData::RuntimeManifest &manifest,
                               uint8_t speciesSlot,
                               uint8_t outfitSlot,
                               PetBehaviorConfig &config)
{
    if (bytes == nullptr || byteCount > UINT32_MAX)
        return false;
    MemorySource memory = {bytes, static_cast<uint32_t>(byteCount)};
    const Source source = {&memory, readMemory, memory.size};
    RuntimeTable table = {};
    PetBehaviorConfig candidate = {};
    if (!readRuntimeTable(source, &manifest, table) ||
        !decodeRuntimeTableBehavior(table, manifest, speciesSlot, outfitSlot, candidate) ||
        !decodeRuntimePresentation(table, speciesSlot, outfitSlot, candidate))
        return false;
    config = candidate;
    return true;
}

bool loadCompleteRuntimeTable(SdFat *sd,
                              const AssetData::RuntimeManifest &manifest,
                              BundleReader &bundleReader,
                              uint8_t speciesSlot,
                              uint8_t outfitSlot,
                              PetBehaviorConfig &config,
                              AppearanceSelection *initialAppearance,
                              bool *initialAppearanceResolved)
{
    // Keep one configuration on memory-constrained targets. Runtime-contract
    // failures are fatal to the session, so decode into the caller-owned buffer
    // instead of retaining a second configuration on the startup stack.
    config = {};
    if (initialAppearance != nullptr)
        *initialAppearance = {};
    if (initialAppearanceResolved != nullptr)
        *initialAppearanceResolved = false;
    RuntimeTableFile tableFile;
    if (!tableFile.open(sd, manifest))
        return false;
    const RuntimeTable &table = tableFile.table();

    AppearanceSelection decodedInitial = {};
    bool decoded = true;
    if (initialAppearance != nullptr)
    {
        decoded = decodeInitialAppearance(table, bundleReader, decodedInitial);
        if (decoded)
        {
            speciesSlot = decodedInitial.speciesSlot;
            outfitSlot = decodedInitial.outfitSlot;
            if (initialAppearanceResolved != nullptr)
                *initialAppearanceResolved = true;
        }
    }
    decoded = decoded &&
              decodeRuntimeTableBehavior(table, manifest, speciesSlot, outfitSlot, config) &&
              decodeRuntimePresentation(table, speciesSlot, outfitSlot, config);
    if (decoded && initialAppearance == nullptr)
        decoded = decodeInitialAppearance(table, bundleReader, decodedInitial);
    if (decoded)
        decoded = AssetData::animationReferenceExists(bundleReader, config.idleAnimation);
    // The former second /runtime.bin read only checked these required
    // appearance sections. Keep that check after used asset references.
    if (decoded)
        decoded = validateAppearanceSections(table);
    if (!decoded)
        return false;
    if (initialAppearance != nullptr)
        *initialAppearance = decodedInitial;
    return true;
}
