#include "pet/PetBehaviorRuntime.h"

#include "animation/AnimationController.h"
#include "animation/Animation.h"
#include "pet/Pet.h"
#include "pet/PetBehaviorRuntimeRules.h"
#include "resources/AssetResources.h"
#include "common/FirmwareRandom.h"
#include "appearance/AppearanceLoader.h"
#include "common/SdBinaryRead.h"
#include "collection/CollectionGameEvent.h"

namespace
{
PetBehaviorStatValues readStats(const Pet &pet)
{
    PetBehaviorStatValues state = {};
    const PetStatSnapshot snapshot = pet.statSnapshot();
    for (uint8_t slot = 0; slot < kPetBehaviorSlotCount; ++slot)
        state.values[slot] = snapshot.customStats[slot];
    state.stageDays = snapshot.stage_days;
    state.speciesSlot = snapshot.speciesSlot;
    return state;
}

bool writeStats(const PetBehaviorStatValues &state,
                Pet &pet)
{
    if (!pet.commitPetStats(state.values, kPetBehaviorSlotCount)) return false;
    pet.setStageDays(state.stageDays);
    return true;
}

uint16_t firmwareRandomBelow(uint16_t upperExclusive)
{
    return FirmwareRandom::below(upperExclusive);
}

PlaybackResult resolveActionAnimation(const PetBehaviorActionPlayback &playback,
                                      AssetResources &resources,
                                      Animation &animation)
{
    if (!playback.animation.valid() || playback.playbackCount == 0)
        return PlaybackResult::PlaybackFailed;
    animation = Animation::complete(playback.animation, playback.playbackCount);
    if (playback.versionSelection != 0)
    {
        animation.versionIndex = playback.versionSelection - 1;
        return resources.frameCountFor(playback.animation, animation.versionIndex) != 0
            ? PlaybackResult::Accepted : PlaybackResult::AnimationMissing;
    }
    const uint16_t versionCount = resources.versionCountFor(playback.animation);
    if (versionCount == 0)
        return PlaybackResult::AnimationMissing;
    animation.versionIndex = versionCount == 1 ? 0 : static_cast<uint8_t>(FirmwareRandom::below(versionCount));
    return PlaybackResult::Accepted;
}
struct ActionEffectFile
{
    SdBaseFile file;
    uint32_t offset;
    SdFat *sd;
    bool opened = false;
    bool failed = false;
    ~ActionEffectFile() { file.close(); }
};

bool readActionEffect(void *context, uint16_t row, PetBehaviorActionEffectConfig &effect)
{
    auto &source = *static_cast<ActionEffectFile *>(context);
    uint8_t record[12];
    if (!source.opened)
    {
        if (source.sd == nullptr || !source.file.open("/runtime.bin", FILE_READ))
        { source.failed = true; return false; }
        source.opened = true;
    }
    if (!source.file.seekSet(source.offset + static_cast<uint32_t>(row) * sizeof(record)) ||
        readSdBinary(source.file, record, sizeof(record)) != static_cast<int>(sizeof(record)))
    { source.failed = true; return false; }
    effect.statSlot = record[0];
    effect.operation = static_cast<PetBehaviorEffectOperation>(record[1]);
    uint64_t value = 0;
    for (uint8_t byte = 0; byte < 8; ++byte) value |= static_cast<uint64_t>(record[2 + byte]) << (8 * byte);
    effect.value = static_cast<int64_t>(value);
    return true;
}
} // namespace

PetBehaviorRuntime::PetBehaviorRuntime(const PetBehaviorConfig &configRef,
                                       Pet &petRef,
                                       AnimationController &animationsRef,
                                       AssetResources &resourcesRef)
    : config(configRef),
      pet(petRef),
      animations(animationsRef),
      resources(resourcesRef),
      dailyChangePauses{}
{
}

bool PetBehaviorRuntime::hasAction(uint8_t actionSlot) const
{
    return actionSlot < kMaxPetBehaviorActions && config.actions[actionSlot].active;
}

void PetBehaviorRuntime::initializeStats()
{
    resetTransientState();
    PetBehaviorStatValues state = readStats(pet);
    initializePetBehaviorStats(config, state);
    writeStats(state, pet);
}

bool PetBehaviorRuntime::catchUpDays(AppearanceLoader &loader, uint32_t limit, uint32_t &days) const
{
    return loader.catchUpDays(config, readStats(pet), dailyChangePauses, limit, days);
}

#if ENABLE_STAT_EVENTS
bool PetBehaviorRuntime::commitStatMutation(const PetBehaviorStatValues &state, uint8_t actionSlot, uint32_t days)
{
    PetStatSnapshot after = pet.statSnapshot();
    for (uint8_t slot = 0; slot < kPetBehaviorSlotCount; ++slot)
        after.customStats[slot] = state.values[slot];
    after.stage_days = state.stageDays;
    return settleStatMutation(saveContext, after, actionSlot, days != 0);
}
#endif

bool PetBehaviorRuntime::advancePetDay(uint32_t days)
{
    const PersistedPetState before = pet.persistentState();
    PetBehaviorStatValues state = readStats(pet);
    PetBehaviorDailyChangePauses nextPauses = dailyChangePauses;
    applyPetBehaviorDailyChanges(config, state, nextPauses, days);
    state.stageDays = applyStageDaysEffect(state.stageDays, PetBehaviorEffectOperation::Change, days);
    for (uint8_t i = 0; i < config.dayResetCount; ++i)
    {
        const auto &effect = config.dayResets[i];
        if (effect.statSlot == kStageDaysEffectSlot)
            state.stageDays = applyStageDaysEffect(state.stageDays, effect.operation, effect.value);
        else if (effect.statSlot < config.statCount)
            state.values[effect.statSlot] = static_cast<uint32_t>(
                effect.value > config.stats[effect.statSlot].maxValue ? config.stats[effect.statSlot].maxValue : effect.value);
        else return false;
    }
#if ENABLE_STAT_EVENTS
    if (usesStatEvents())
    {
        if (!commitStatMutation(state, UINT8_MAX, days)) return false;
    }
    else
#endif
    {
        if (!writeStats(state, pet)) return false;
        if (
#if ENABLE_COLLECTION_GAME_EVENTS
            !recordCollectionGameEvent(resources.sdCard(), config, pet, before, CollectionGameEvent::StatTransition) ||
#endif
            (config.manualDays && saveMutation != nullptr && !saveMutation(saveContext)))
        {
            pet.restoreState(before);
            return false;
        }
    }
    dailyChangePauses = nextPauses;
    return true;
}

PetBehaviorActionResult PetBehaviorRuntime::executeAction(uint8_t actionSlot)
{
    if (pet.isDead() || pet.pendingActionOutcome() != UINT8_MAX || !hasAction(actionSlot))
        return PetBehaviorActionResult::Rejected;

    PetBehaviorStatValues state = readStats(pet);
    if (config.actionGateCount)
    {
        ActionEffectFile gateSource;
        SdBaseFile &file = gateSource.file;
        if (!file.open("/runtime.bin", FILE_READ) || !file.seekSet(config.actionGatesOffset))
            return PetBehaviorActionResult::ResourceReadFailed;
        for (uint16_t i = 0; i < config.actionGateCount; ++i)
        {
            uint8_t record[12];
            if (readSdBinary(file, record, sizeof(record)) != sizeof(record))
                return PetBehaviorActionResult::ResourceReadFailed;
            if (record[0] != actionSlot) continue;
            const uint32_t left = record[1] == 255 ? state.stageDays : state.values[record[1]];
            uint32_t right = 0;
            for (uint8_t b = 0; b < 4; ++b) right |= static_cast<uint32_t>(record[4 + b]) << (8 * b);
            if (record[3]) right = right == 255 ? state.stageDays : state.values[right];
            const bool accepted = record[2] == 0 ? left < right : record[2] == 1 ? left <= right :
                record[2] == 2 ? left == right : record[2] == 3 ? left >= right : left > right;
            if (!accepted) return PetBehaviorActionResult::Rejected;
        }
        file.close();
    }
    const PersistedPetState before = pet.persistentState();
    PetBehaviorDailyChangePauses nextPauses = dailyChangePauses;
    PetBehaviorActionPlayback playback = {};
    uint8_t selectedOutcome = UINT8_MAX;
    ActionEffectFile effectSource;
    effectSource.offset = config.actionEffectsOffset;
    effectSource.sd = resources.sdCard();
    if (!applyPetBehaviorAction(
            config, actionSlot, state, nextPauses, playback, firmwareRandomBelow,
            readActionEffect, &effectSource, &selectedOutcome))
        return effectSource.failed ? PetBehaviorActionResult::ResourceReadFailed : PetBehaviorActionResult::Rejected;
    const auto &action = config.actions[actionSlot];
    if (action.effectsAfterPlayback || playback.evolutionSpecies)
        pet.setPendingActionOutcome(selectedOutcome);
    if (action.effectsAfterPlayback)
    {
        if (saveMutation != nullptr && !saveMutation(saveContext))
        {
            pet.restoreState(before);
            return PetBehaviorActionResult::ResourceReadFailed;
        }
        pendingPlaybackStarted = playAction(action, playback) == PlaybackResult::Accepted;
        return PetBehaviorActionResult::AwaitingCompletion;
    }

    const auto committed = commitActionValues(state, actionSlot, before);
    if (committed != PetBehaviorActionResult::Applied) return committed;
    dailyChangePauses = nextPauses;
    if (pet.isDead()) return PetBehaviorActionResult::Applied;

    const PlaybackResult result = playAction(action, playback);
    pendingPlaybackStarted = playback.evolutionSpecies && result == PlaybackResult::Accepted;
    return result == PlaybackResult::AnimationMissing
           ? PetBehaviorActionResult::AppliedAnimationMissing : PetBehaviorActionResult::Applied;
}

PetBehaviorActionResult PetBehaviorRuntime::commitActionValues(
    const PetBehaviorStatValues &state, uint8_t actionSlot, const PersistedPetState &before)
{
#if ENABLE_STAT_EVENTS
    if (usesStatEvents())
    {
        if (!commitStatMutation(state, actionSlot))
        {
            pet.restoreState(before);
            return PetBehaviorActionResult::ResourceReadFailed;
        }
    }
    else
#endif
    {
        if (!writeStats(state, pet))
        {
            pet.restoreState(before);
            return PetBehaviorActionResult::Rejected;
        }
#if ENABLE_COLLECTION_CATALOG
        for (uint8_t index = 0; index < config.trackedActionCount; ++index)
            if (config.trackedActions[index] == actionSlot) { pet.incrementActionCount(index); break; }
#endif
        if (
#if ENABLE_COLLECTION_GAME_EVENTS
            !recordCollectionGameEvent(resources.sdCard(), config, pet, before, CollectionGameEvent::StatTransition) ||
#endif
            (saveMutation != nullptr && !saveMutation(saveContext)))
        {
            pet.restoreState(before);
            return PetBehaviorActionResult::ResourceReadFailed;
        }
    }
    return PetBehaviorActionResult::Applied;
}

PlaybackResult PetBehaviorRuntime::playAction(const PetBehaviorActionConfig &action,
                                             const PetBehaviorActionPlayback &playback)
{
    Animation sequence[2 + kMaxPetBehaviorFollowingAnimationsPerAction];
    uint8_t count = 0;
    if (action.followingAnimationCount > kMaxPetBehaviorFollowingAnimationsPerAction ||
        static_cast<uint16_t>(action.firstFollowingAnimation) + action.followingAnimationCount >
            kMaxPetBehaviorFollowingAnimations)
        return PlaybackResult::PlaybackFailed;
    if (action.prelude.animation.valid())
    {
        const PlaybackResult result = resolveActionAnimation(action.prelude, resources, sequence[count]);
        if (result != PlaybackResult::Accepted) return result;
        ++count;
    }
    for (uint8_t index = 0; index <= action.followingAnimationCount; ++index)
    {
        const auto &segment = index == 0 ? playback :
            config.followingAnimations[action.firstFollowingAnimation + index - 1];
        if (index == 0 && !segment.animation.valid())
            continue;
        const PlaybackResult buildResult = resolveActionAnimation(segment, resources, sequence[count]);
        if (buildResult != PlaybackResult::Accepted)
            return buildResult;
        ++count;
    }

    if (count == 0)
        return PlaybackResult::Accepted;

    AnimationSequence request(sequence, count);
    return animations.replace(request);
}

PendingActionProgress PetBehaviorRuntime::continuePendingAction(uint8_t &species, uint8_t &outfit)
{
    const uint8_t index = pet.pendingActionOutcome();
    for (uint8_t slot = 0; slot < config.actionCount; ++slot)
    {
        const auto &action = config.actions[slot];
        if (index < action.firstOutcome || index >= action.firstOutcome + action.outcomeCount)
            continue;
        const auto &playback = config.actionOutcomes[index].animationPlayback;
        if (!playback.evolutionSpecies && !action.effectsAfterPlayback)
            break;
        if (pendingPlaybackStarted && animations.hasFinitePlayback())
            return PendingActionProgress::Playing;
        if (action.effectsAfterPlayback)
        {
            const PersistedPetState before = pet.persistentState();
            auto state = readStats(pet);
            auto nextPauses = dailyChangePauses;
            PetBehaviorActionPlayback selected = {};
            ActionEffectFile effectSource;
            effectSource.offset = config.actionEffectsOffset;
            effectSource.sd = resources.sdCard();
            if (!applyPetBehaviorAction(config, slot, state, nextPauses, selected, nullptr, readActionEffect, &effectSource))
                return PendingActionProgress::Failed;
            if (!pendingPlaybackStarted)
            {
                if (playAction(action, selected) != PlaybackResult::Accepted)
                    return PendingActionProgress::Failed;
                pendingPlaybackStarted = true;
                return PendingActionProgress::Playing;
            }
            pendingPlaybackStarted = false;
            pet.setPendingActionOutcome(UINT8_MAX);
            if (commitActionValues(state, slot, before) != PetBehaviorActionResult::Applied)
                return PendingActionProgress::Failed;
            dailyChangePauses = nextPauses;
            return PendingActionProgress::Settled;
        }
        if (!pendingPlaybackStarted)
        {
            if (playAction(action, playback) != PlaybackResult::Accepted)
                return PendingActionProgress::Failed;
            pendingPlaybackStarted = true;
            return PendingActionProgress::Playing;
        }
        pendingPlaybackStarted = false;
        species = playback.evolutionSpecies;
        outfit = playback.evolutionOutfit;
        return PendingActionProgress::Evolve;
    }
    return PendingActionProgress::Failed;
}

#if ENABLE_GUESS_GAME
bool PetBehaviorRuntime::applyGuessOutcome(PetBehaviorGuessOutcome outcome)
{
    if (pet.isDead()) return false;
    PetBehaviorStatValues state = readStats(pet);
    if (!applyPetBehaviorGuessOutcome(config, outcome, state))
        return false;
#if ENABLE_STAT_EVENTS
    if (usesStatEvents()) return commitStatMutation(state, UINT8_MAX);
#endif
    return writeStats(state, pet);
}
#endif

AssetData::AnimationRef PetBehaviorRuntime::baseAnimation() const
{
#if ENABLE_DEATH
    if (pet.isDead()) return config.deathAnimation.valid() ? config.deathAnimation : config.idleAnimation;
#else
    if (pet.isDead()) return config.idleAnimation;
#endif
    return activePetState().idleAnimation;
}

bool PetBehaviorRuntime::isDead() const { return pet.isDead(); }

ActivePetState PetBehaviorRuntime::activePetState() const
{
    return activePetState(pet.statSnapshot());
}

ActivePetState PetBehaviorRuntime::activePetState(
    const PetStatSnapshot &snapshot) const
{
    return PetStateClassifier::classify(config, snapshot);
}

#if ENABLE_INSTRUCTION_RESPONSE
bool PetBehaviorRuntime::applyInstructionResults(uint32_t correctMask, uint8_t count)
{
    if (pet.isDead()) return false;
    PetBehaviorStatValues state = readStats(pet);
    if (!applyPetBehaviorInstructionResults(config, correctMask, count, state)) return false;
#if ENABLE_STAT_EVENTS
    if (usesStatEvents()) return commitStatMutation(state, UINT8_MAX);
#endif
    return writeStats(state, pet);
}
#endif
