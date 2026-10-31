#include "resources/detail/RuntimeTableDecoders.h"

#include <string.h>
#include "controller/SystemCommandCatalog.h"
#include "pet/RuntimeValueResolver.h"
#if ENABLE_COLLECTION_CATALOG
#include "collection/CollectionCatalog.h"
#endif

namespace RuntimeTableInternal
{
namespace
{
bool decodeStats(const Source &source, const Section &section, PetBehaviorConfig &config)
{
    if (section.count == 0 || section.count > kPetBehaviorSlotCount)
        return false;
    for (uint16_t index = 0; index < section.count; ++index)
    {
        uint8_t record[24] = {};
        if (!readRecord(source, section, index, record))
            return false;
        PetBehaviorStatConfig &stat = config.stats[index];
        stat.active = true;
        stat.initialValue = readU32(record + 2);
        stat.minValue = readU32(record + 6);
        stat.maxValue = readU32(record + 10);
        stat.dailyChange = readI64(record + 14);
    }
    config.statCount = static_cast<uint8_t>(section.count);
    return true;
}

#if ENABLE_DEATH
bool decodeDeathAnimation(const Source &source, const Section &assets,
                          const Section &animations, uint16_t reference,
                          const ActiveAssetScope &scope, PetBehaviorConfig &config)
{
    if (reference == kNone16) return true;
    uint8_t animation[8] = {};
    if (reference >= animations.count || !readRecord(source, animations, reference, animation)) return false;
    const uint16_t first = readU16(animation + 4), count = readU16(animation + 6);
    if (static_cast<uint32_t>(first) + count > assets.count) return false;
    for (uint16_t offset = 0; offset < count; ++offset)
    {
        uint8_t asset[12] = {};
        if (!readRecord(source, assets, static_cast<uint16_t>(first + offset), asset)) return false;
        if (asset[2] == 2 || (asset[2] == 1 && asset[3] == scope.speciesSlot && asset[4] == scope.outfitSlot))
            return resolveAnimation(source, assets, animations, reference, scope, config.deathAnimation);
    }
    return !config.deathEnabled;
}
#endif

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
        uint8_t record[24] = {};
        AssetData::AnimationRef animation = {};
        if (!readRecord(source, *states, index, record))
            return false;
        const RuntimeRangePredicate predicate = {
            readU16(record + 2), readI64(record + 4), readI64(record + 12)};
        if (!resolveAnimation(source, assets, animations, readU16(record + 20), scope, animation))
            return false;

        RuntimePetStateConfig &state = config.petStates[index];
        state.predicate = predicate;
        state.idleAnimation = animation;
    }

    uint8_t record[4] = {};
    if (!readRecord(source, *defaultState, 0, record) ||
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
        uint8_t record[14] = {};
        if (!readRecord(source, *effects, index, record) || record[1] >= config.statCount)
            return false;

        PetBehaviorGuessEffectConfig &effect = config.guessEffects[index];
        effect.active = true;
        effect.outcome = static_cast<PetBehaviorGuessOutcome>(record[0]);
        effect.statSlot = record[1];
        effect.operation = static_cast<PetBehaviorEffectOperation>(record[2]);
        effect.value = readI64(record + 4);
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
    config.actionEffectsStreamed = effects != nullptr && effects->count > kStoredPetBehaviorActionEffects;
    config.actionEffectsOffset = effects == nullptr ? 0 : effects->offset;
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
        if (outcomeCount > kMaxPetBehaviorLotteryOutcomesPerAction ||
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
                static_cast<uint32_t>(nextEffect) + readU16(outcomeRecord + 6) >
                    (effects == nullptr ? 0 : effects->count))
                return false;
            const uint8_t weight = outcomeRecord[0];
            const uint8_t playbackCount = outcomeRecord[1];
            const uint16_t animationRef = readU16(outcomeRecord + 2);
            const uint16_t effectCount = readU16(outcomeRecord + 6);
            if (effectCount > kPetBehaviorSlotCount)
                return false;
            AssetData::AnimationRef animation = {};
            if (animationRef != kNone16 &&
                !resolveAnimation(source, assets, animations, animationRef, scope, animation))
                return false;

            PetBehaviorActionOutcomeConfig &outcome = config.actionOutcomes[nextOutcome];
            outcome.weight = weight;
            outcome.animationPlayback.animation = animation;
            outcome.animationPlayback.playbackCount = playbackCount;
            outcome.animationPlayback.versionSelection = readU16(outcomeRecord + 8);
            outcome.firstEffect = nextEffect;
            outcome.effectCount = static_cast<uint8_t>(effectCount);

            for (uint16_t effectOffset = 0; effectOffset < effectCount; ++effectOffset)
            {
                if (!config.actionEffectsStreamed)
                {
                    uint8_t effectRecord[12] = {};
                    if (effects == nullptr || !readRecord(source, *effects, nextEffect, effectRecord))
                        return false;
                    PetBehaviorActionEffectConfig &effect = config.actionEffects[nextEffect];
                    effect.statSlot = effectRecord[0];
                    effect.operation = static_cast<PetBehaviorEffectOperation>(effectRecord[1]);
                    effect.value = readI64(effectRecord + 2);
                }
                ++nextEffect;
            }
            ++nextOutcome;
        }

        for (uint8_t conditionSlot = 0; conditionSlot < conditionCount; ++conditionSlot)
        {
            uint8_t conditionRecord[24] = {};
            if (conditions == nullptr ||
                !readRecord(source, *conditions, nextCondition, conditionRecord))
                return false;
            const RuntimeRangePredicate predicate = {
                readU16(conditionRecord + 2), readI64(conditionRecord + 8),
                readI64(conditionRecord + 16)};
            PetBehaviorActionConditionConfig &condition =
                config.actionConditions[nextCondition];
            condition.priority = conditionRecord[0];
            condition.predicate = predicate;
            condition.animationPlayback.playbackCount = conditionRecord[4];
            if (readU16(conditionRecord + 6) != kNone16 &&
                !resolveAnimation(source, assets, animations, readU16(conditionRecord + 6), scope,
                                  condition.animationPlayback.animation))
                return false;
            ++nextCondition;
        }
    }
    config.actionCount = static_cast<uint8_t>(actions.count);
    config.actionOutcomeCount = static_cast<uint8_t>(nextOutcome);
    config.actionConditionCount = static_cast<uint8_t>(nextCondition);
    config.actionEffectCount = nextEffect;
    return true;
}

bool decodeFollowingAnimations(const Source &source, const Section *playbacks,
                               const Section &assets, const Section &animations,
                               const ActiveAssetScope &scope, PetBehaviorConfig &config)
{
    if (playbacks == nullptr) return true;
    if (playbacks->count > kMaxPetBehaviorFollowingAnimations + kMaxPetBehaviorActions) return false;
    uint8_t followingCount = 0;
    // Ordering, ordinal ownership and playback semantics are exporter-owned.
    // Firmware only bounds the referenced storage and resolves used resources.
    for (uint16_t index = 0; index < playbacks->count; ++index)
    {
        uint8_t record[6] = {};
        if (!readRecord(source, *playbacks, index, record) || record[0] >= config.actionCount)
            return false;
        PetBehaviorActionConfig &action = config.actions[record[0]];
        if (record[5] == 1)
        {
            action.prelude.playbackCount = record[4];
            if (!resolveAnimation(source, assets, animations, readU16(record + 2), scope,
                                  action.prelude.animation)) return false;
            continue;
        }
        if (followingCount >= kMaxPetBehaviorFollowingAnimations ||
            action.followingAnimationCount >= kMaxPetBehaviorFollowingAnimationsPerAction)
            return false;
        if (action.followingAnimationCount == 0)
            action.firstFollowingAnimation = followingCount;
        PetBehaviorAnimationPlaybackConfig &playback = config.followingAnimations[followingCount++];
        playback.playbackCount = record[4];
        if (!resolveAnimation(source, assets, animations, readU16(record + 2), scope,
                              playback.animation))
            return false;
        ++action.followingAnimationCount;
    }
    return true;
}

bool decodeButtons(const Source &source, const Section &buttons, PetBehaviorConfig &config)
{
    if (buttons.count != kPetBehaviorButtonCount)
        return false;
    for (uint16_t index = 0; index < buttons.count; ++index)
    {
        uint8_t record[8] = {};
        if (!readRecord(source, buttons, index, record))
            return false;
        PetBehaviorButtonConfig &button = config.buttons[index];
        button.active = true;
        button.kind = static_cast<PetBehaviorButtonKind>(record[1]);
        const uint16_t target = readU16(record + 2);
        if (button.kind == PetBehaviorButtonKind::UserAction)
        {
            if (target >= config.actionCount)
                return false;
            button.actionSlot = static_cast<uint8_t>(target);
        }
        else if (button.kind == PetBehaviorButtonKind::SystemCommand)
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
            setRecord[1] > kMaxStatusConditions ||
            static_cast<uint32_t>(nextCondition) + setRecord[1] > conditions->count)
            return false;
        StatusSetConfig &set = config.statusSets.sets[setIndex];
        set.conditionCount = setRecord[1];
        set.versionCount = readU16(setRecord + 6);
        if (!resolveAnimation(source, assets, animations, readU16(setRecord + 2),
                              scope, set.animation))
            return false;
        for (uint8_t conditionIndex = 0; conditionIndex < set.conditionCount; ++conditionIndex)
        {
            uint8_t conditionRecord[20] = {};
            if (!readRecord(source, *conditions, nextCondition, conditionRecord))
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
            condition.minValue = readI64(conditionRecord + 4);
            condition.maxValue = readI64(conditionRecord + 12);
            ++nextCondition;
        }
    }
    config.statusSets.count = static_cast<uint8_t>(sets->count);
    return true;
}

bool decodeAnimationAudio(const RuntimeTable &table, const Section &animations,
                          PetBehaviorConfig &config)
{
#if ENABLE_SEQUENCE_AUDIO
    const Section *bindings = table.find(AnimationAudio);
    if (bindings == nullptr) return true;
    const Section *references = table.find(AudioRefs);
    if (references == nullptr || bindings->count > AssetData::kMaxRuntimeAnimationId) return false;
    for (uint16_t index = 0; index < bindings->count; ++index)
    {
        uint8_t binding[4] = {}, animation[8] = {}, resource[12] = {};
        if (!readRecord(table.source, *bindings, index, binding) ||
            !readRecord(table.source, animations, readU16(binding), animation) ||
            !readRecord(table.source, *references, readU16(binding + 2), resource)) return false;
        const uint16_t runtimeId = readU16(animation + 2);
        // Bound only the storage actually dereferenced; exporter validates ownership.
        if (runtimeId == 0 || runtimeId > AssetData::kMaxRuntimeAnimationId) return false;
        config.animationAudio[runtimeId - 1] = {readU32(resource + 4), readU32(resource + 8)};
    }
#else
    if ((table.featureFlags & kSequenceAudioFeature) != 0) return false;
#endif
    return true;
}

} // namespace

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

    // The caller owns initialization and discards the configuration on failure.
    config.assetManifest = manifest;
    config.activeSpeciesSlot = speciesSlot;
    config.activeOutfitSlot = outfitSlot;
    config.schemaFingerprint = table.schemaFingerprint;
#if ENABLE_COLLECTION_CATALOG
    if (!loadCollectionCatalog(table, config))
        return false;
#else
    if ((table.featureFlags & kCollectionFeature) != 0) return false;
#endif
    const ActiveAssetScope scope = {speciesSlot, outfitSlot};
#if ENABLE_INSTRUCTION_RESPONSE
    if (table.featureFlags & kInstructionResponseFeature)
    {
        const Section *instruction = table.find(InstructionResponseConfig);
        uint8_t record[24] = {};
        if (instruction == nullptr || !readRecord(source, *instruction, 0, record)) return false;
        config.instructionSettings.roundCount = record[0];
        config.instructionSettings.hard = record[1] != 0;
        config.instructionSettings.noGoEnabled = (readU16(record + 6) & 1) != 0;
        config.instructionSettings.typeWeights[0] = readU16(record + 20);
        config.instructionSettings.typeWeights[1] = readU16(record + 22);
        config.instructionSettings.answerMs = readU16(record + 2);
        config.instructionSettings.feedbackMs = readU16(record + 4);
        for (uint8_t item = 0; item < 3; ++item)
        {
            config.instructionSettings.targetWeights[item] = readU16(record + 14 + item * 2);
            if (!resolveAnimation(source, *assets, *animations, readU16(record + 8 + item * 2),
                                  scope, config.instructionOptions[item])) return false;
        }
    }
#endif
#if ENABLE_INSTRUCTION_RESPONSE
    const Section *instructionEffects = table.find(InstructionResponseEffects);
    if (instructionEffects != nullptr)
    {
        for (uint16_t index = 0; index < instructionEffects->count; ++index)
        {
            uint8_t record[12];
            if (!readRecord(source, *instructionEffects, index, record)) return false;
            auto &effect = config.instructionEffects[index];
            effect.group = record[0];
            effect.statSlot = record[1];
            effect.operation = static_cast<PetBehaviorEffectOperation>(record[2]);
            effect.value = readI64(record + 4);
        }
        config.instructionEffectCount = static_cast<uint8_t>(instructionEffects->count);
    }
#endif
    const Section *death = table.find(DeathConfig);
    uint8_t deathRecord[8] = {};
    if (death == nullptr || death->count != 1 || !readRecord(source, *death, 0, deathRecord))
        return false;
#if ENABLE_DEATH
    config.deathEnabled = deathRecord[0] != 0;
    config.deathStatMask = readU16(deathRecord + 2);
    const uint16_t deathRef = readU16(deathRecord + 4);
    if (!decodeDeathAnimation(source, *assets, *animations, deathRef, scope, config))
        return false;
#else
    if (deathRecord[0] != 0) return false;
#endif
    if (!decodeStats(source, *stats, config) ||
        !decodePetStates(source, petStates, defaultPetState,
                         *assets, *animations, scope, config) ||
        !decodeActions(source, *actions, *outcomes, actionConditions, effects,
                       *assets, *animations, scope, config) ||
        !decodeFollowingAnimations(source, table.find(ActionPlaybacks),
                                   *assets, *animations, scope, config) ||
        !decodeAnimationAudio(table, *animations, config) ||
        !decodeGuessEffects(source, table.featureFlags, guessEffects, config) ||
        !decodeButtons(source, *buttons, config) ||
        !decodeStatus(source, table.featureFlags,
                      table.find(StatusSets), table.find(StatusConditions),
                      *assets, *animations, scope, config))
        return false;
    return true;
}

} // namespace RuntimeTableInternal
