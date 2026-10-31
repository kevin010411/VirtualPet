#include "pet/PetBehaviorRuntime.h"

#include "animation/AnimationController.h"
#include "animation/Animation.h"
#include "pet/Pet.h"
#include "pet/PetBehaviorRuntimeRules.h"
#include "resources/AssetResources.h"
#include "common/FirmwareRandom.h"
#include "appearance/AppearanceLoader.h"
#include "common/SdBinaryRead.h"

namespace
{
PetBehaviorStatValues readStats(const Pet &pet)
{
    PetBehaviorStatValues state = {};
    const PetStatSnapshot snapshot = pet.statSnapshot();
    for (uint8_t slot = 0; slot < kPetBehaviorSlotCount; ++slot)
        state.values[slot] = snapshot.customStats[slot];
    state.stageDays = snapshot.stage_days;
    return state;
}

bool writeStats(const PetBehaviorStatValues &state,
                Pet &pet)
{
    return pet.commitPetStats(state.values, kPetBehaviorSlotCount);
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
    const uint16_t versionCount = resources.versionCountFor(playback.animation);
    if (versionCount == 0)
        return PlaybackResult::AnimationMissing;
    animation = Animation::complete(playback.animation, playback.playbackCount);
    if (playback.versionSelection > versionCount) return PlaybackResult::AnimationMissing;
    animation.versionIndex = playback.versionSelection != 0 ? playback.versionSelection - 1 :
        (versionCount == 1 ? 0 : static_cast<uint8_t>(FirmwareRandom::below(versionCount)));
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

bool PetBehaviorRuntime::advancePetDay(uint32_t days)
{
    PetBehaviorStatValues state = readStats(pet);
    PetBehaviorDailyChangePauses nextPauses = dailyChangePauses;
    applyPetBehaviorDailyChanges(config, state, nextPauses, days);
    if (!pet.commitPetDay(state.values, kPetBehaviorSlotCount, days)) return false;
    dailyChangePauses = nextPauses;
    return true;
}

PetBehaviorActionResult PetBehaviorRuntime::executeAction(uint8_t actionSlot)
{
    if (pet.isDead() || !hasAction(actionSlot))
        return PetBehaviorActionResult::Rejected;

    PetBehaviorStatValues state = readStats(pet);
    PetBehaviorDailyChangePauses nextPauses = dailyChangePauses;
    PetBehaviorActionPlayback playback = {};
    ActionEffectFile effectSource;
    effectSource.offset = config.actionEffectsOffset;
    effectSource.sd = resources.sdCard();
    if (!applyPetBehaviorAction(
            config, actionSlot, state, nextPauses, playback, firmwareRandomBelow,
            readActionEffect, &effectSource))
        return effectSource.failed ? PetBehaviorActionResult::ResourceReadFailed : PetBehaviorActionResult::Rejected;
    if (!writeStats(state, pet))
        return PetBehaviorActionResult::Rejected;
    dailyChangePauses = nextPauses;
#if ENABLE_COLLECTION_CATALOG
    for (uint8_t index = 0; index < config.trackedActionCount; ++index)
        if (config.trackedActions[index] == actionSlot) { pet.incrementActionCount(index); break; }
#endif
    if (pet.isDead()) return PetBehaviorActionResult::Applied;

    const PetBehaviorActionConfig &action = config.actions[actionSlot];
    Animation sequence[2 + kMaxPetBehaviorFollowingAnimationsPerAction];
    uint8_t count = 0;
    if (action.followingAnimationCount > kMaxPetBehaviorFollowingAnimationsPerAction ||
        static_cast<uint16_t>(action.firstFollowingAnimation) + action.followingAnimationCount >
            kMaxPetBehaviorFollowingAnimations)
        return PetBehaviorActionResult::Applied;
    if (action.prelude.animation.valid())
    {
        const PlaybackResult result = resolveActionAnimation(action.prelude, resources, sequence[count]);
        if (result == PlaybackResult::AnimationMissing) return PetBehaviorActionResult::AppliedAnimationMissing;
        if (result != PlaybackResult::Accepted) return PetBehaviorActionResult::Applied;
        ++count;
    }
    for (uint8_t index = 0; index <= action.followingAnimationCount; ++index)
    {
        const auto &segment = index == 0 ? playback :
            config.followingAnimations[action.firstFollowingAnimation + index - 1];
        if (index == 0 && !segment.animation.valid())
            continue;
        const PlaybackResult buildResult = resolveActionAnimation(segment, resources, sequence[count]);
        if (buildResult == PlaybackResult::AnimationMissing)
            return PetBehaviorActionResult::AppliedAnimationMissing;
        if (buildResult != PlaybackResult::Accepted)
            return PetBehaviorActionResult::Applied;
        ++count;
    }

    if (count == 0)
        return PetBehaviorActionResult::Applied;

    AnimationSequence request(sequence, count);
    const PlaybackResult replaceResult = animations.replace(request);
    return replaceResult == PlaybackResult::AnimationMissing
               ? PetBehaviorActionResult::AppliedAnimationMissing
               : PetBehaviorActionResult::Applied;
}

#if ENABLE_GUESS_GAME
bool PetBehaviorRuntime::applyGuessOutcome(PetBehaviorGuessOutcome outcome)
{
    if (pet.isDead()) return false;
    PetBehaviorStatValues state = readStats(pet);
    if (!applyPetBehaviorGuessOutcome(config, outcome, state))
        return false;
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
    return writeStats(state, pet);
}
#endif
