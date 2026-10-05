#include "pet_behavior/application/PetBehaviorRuntime.h"

#include "animation/application/AnimationController.h"
#include "animation/domain/Animation.h"
#include "pet/domain/Pet.h"
#include "pet_behavior/domain/PetBehaviorRuntimeRules.h"
#include "presentation/adapters/rendering/Renderer.h"
#include "shared/utils/FirmwareRandom.h"

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
    if (!hasAction(actionSlot))
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

    Animation animation;
    const PlaybackResult buildResult = resolveActionAnimation(playback, renderer, animation);
    if (buildResult == PlaybackResult::AnimationMissing)
        return PetBehaviorActionResult::AppliedAnimationMissing;
    else if (buildResult != PlaybackResult::Accepted)
        return PetBehaviorActionResult::Applied;

    const PlaybackResult replaceResult = animations.replace(AnimationSequence(&animation, 1));
    return replaceResult == PlaybackResult::AnimationMissing
               ? PetBehaviorActionResult::AppliedAnimationMissing
               : PetBehaviorActionResult::Applied;
}

#if ENABLE_GUESS_GAME
bool PetBehaviorRuntime::applyGuessOutcome(PetBehaviorGuessOutcome outcome)
{
    PetBehaviorStatValues state = readStats(pet);
    if (!applyPetBehaviorGuessOutcome(config, outcome, state))
        return false;
    return writeStats(state, pet);
}
#endif

AssetData::AnimationRef PetBehaviorRuntime::baseAnimation() const
{
    return activePetState().idleAnimation;
}

ActivePetState PetBehaviorRuntime::activePetState() const
{
    return activePetState(pet.statSnapshot());
}

ActivePetState PetBehaviorRuntime::activePetState(
    const PetStatSnapshot &snapshot) const
{
    return PetStateClassifier::classify(config, snapshot);
}
