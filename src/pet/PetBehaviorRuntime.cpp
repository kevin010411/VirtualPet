#include "pet/PetBehaviorRuntime.h"

#include "animation/AnimationController.h"
#include "animation/Animation.h"
#include "pet/Pet.h"
#include "pet/PetBehaviorRuntimeRules.h"
#include "display/Renderer.h"
#include "common/FirmwareRandom.h"

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
                                      Renderer &renderer,
                                      Animation &animation)
{
    if (!playback.animation.valid() || playback.playbackCount == 0)
        return PlaybackResult::PlaybackFailed;
    const uint16_t versionCount = renderer.versionCountFor(playback.animation);
    if (versionCount == 0)
        return PlaybackResult::AnimationMissing;
    animation = Animation::complete(playback.animation, playback.playbackCount);
    animation.versionIndex = versionCount == 1 ? 0 : static_cast<uint8_t>(FirmwareRandom::below(versionCount));
    return PlaybackResult::Accepted;
}
} // namespace

PetBehaviorRuntime::PetBehaviorRuntime(const PetBehaviorConfig &configRef,
                                       Pet &petRef,
                                       AnimationController &animationsRef,
                                       Renderer &rendererRef)
    : config(configRef),
      pet(petRef),
      animations(animationsRef),
      renderer(rendererRef),
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

bool PetBehaviorRuntime::advancePetDay()
{
    PetBehaviorStatValues state = readStats(pet);
    applyPetBehaviorDailyChanges(config, state, dailyChangePauses);
    return pet.commitPetDay(state.values, kPetBehaviorSlotCount);
}

PetBehaviorActionResult PetBehaviorRuntime::executeAction(uint8_t actionSlot)
{
    if (pet.isDead() || !hasAction(actionSlot))
        return PetBehaviorActionResult::Rejected;

    PetBehaviorStatValues state = readStats(pet);
    PetBehaviorDailyChangePauses nextPauses = dailyChangePauses;
    PetBehaviorActionPlayback playback = {};
    if (!applyPetBehaviorAction(
            config, actionSlot, state, nextPauses, playback, firmwareRandomBelow))
        return PetBehaviorActionResult::Rejected;
    if (!writeStats(state, pet))
        return PetBehaviorActionResult::Rejected;
    dailyChangePauses = nextPauses;
#if ENABLE_COLLECTION_CATALOG
    for (uint8_t index = 0; index < config.trackedActionCount; ++index)
        if (config.trackedActions[index] == actionSlot) { pet.incrementActionCount(index); break; }
#endif
    if (pet.isDead()) return PetBehaviorActionResult::Applied;

    const PetBehaviorActionConfig &action = config.actions[actionSlot];
    Animation sequence[1 + kMaxPetBehaviorFollowingAnimationsPerAction];
    uint8_t count = 0;
    if (action.followingAnimationCount > kMaxPetBehaviorFollowingAnimationsPerAction ||
        static_cast<uint16_t>(action.firstFollowingAnimation) + action.followingAnimationCount >
            kMaxPetBehaviorFollowingAnimations)
        return PetBehaviorActionResult::Applied;
    for (uint8_t index = 0; index <= action.followingAnimationCount; ++index)
    {
        const auto &segment = index == 0 ? playback :
            config.followingAnimations[action.firstFollowingAnimation + index - 1];
        if (index == 0 && !segment.animation.valid())
            continue;
        const PlaybackResult buildResult = resolveActionAnimation(segment, renderer, sequence[count]);
        if (buildResult == PlaybackResult::AnimationMissing)
            return PetBehaviorActionResult::AppliedAnimationMissing;
        if (buildResult != PlaybackResult::Accepted)
            return PetBehaviorActionResult::Applied;
        ++count;
    }

    if (count == 0)
        return PetBehaviorActionResult::Applied;

    AnimationSequence request(sequence, count);
#if ENABLE_SEQUENCE_AUDIO
    request.audio = config.actionAudio[actionSlot];
#endif
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
