#include "pet_behavior/application/PetBehaviorRuntime.h"

#include "animation/application/AnimationController.h"
#include "animation/domain/Animation.h"
#include "pet/application/PetActionController.h"
#include "pet_behavior/domain/PetBehaviorRuntimeRules.h"
#include "presentation/adapters/rendering/Renderer.h"

namespace
{
PetBehaviorStatValues readStats(const PetActionController &petActions)
{
    PetBehaviorStatValues state = {};
    const PetStatSnapshot snapshot = petActions.statSnapshot();
    for (uint8_t slot = 0; slot < kPetBehaviorSlotCount; ++slot)
        state.values[slot] = snapshot.customStats[slot];
    state.stageDays = snapshot.stage_days;
    return state;
}

bool writeStats(const PetBehaviorStatValues &state,
                PetActionController &petActions)
{
    return petActions.commitPetStats(state.values, kPetBehaviorSlotCount);
}

uint16_t arduinoRandomBelow(uint16_t upperExclusive)
{
    return static_cast<uint16_t>(random(upperExclusive));
}

PlaybackResult resolveActionAnimation(const PetBehaviorActionPlayback &playback,
                                      Renderer &renderer,
                                      Animation &animation)
{
    if (playback.animation[0] == '\0' || playback.playbackCount == 0)
        return PlaybackResult::PlaybackFailed;

    const char *selectedAnimation = playback.animation;
    const uint8_t variantCount = renderer.variantCountFor(playback.animation);
    if (variantCount > 0)
    {
        selectedAnimation = renderer.variantNameFor(
            playback.animation,
            static_cast<uint8_t>(random(variantCount)));
        if (selectedAnimation == nullptr || renderer.frameCountForName(selectedAnimation) == 0)
            return PlaybackResult::AnimationMissing;

        animation = Animation::complete(selectedAnimation, playback.playbackCount);
        return animation.namedAnimation[0] == '\0'
                   ? PlaybackResult::PlaybackFailed
                   : PlaybackResult::Accepted;
    }

    const AnimationId animationId = animationIdFromName(playback.animation);
    if (animationId != AnimationId::None)
    {
        if (renderer.frameCountFor(animationId) == 0)
            return PlaybackResult::AnimationMissing;
        animation = Animation::complete(animationId, playback.playbackCount);
        return PlaybackResult::Accepted;
    }

    if (renderer.frameCountForName(playback.animation) == 0)
        return PlaybackResult::AnimationMissing;
    animation = Animation::complete(playback.animation, playback.playbackCount);
    return animation.namedAnimation[0] == '\0'
               ? PlaybackResult::PlaybackFailed
               : PlaybackResult::Accepted;
}
} // namespace

PetBehaviorRuntime::PetBehaviorRuntime(const PetBehaviorConfig &configRef,
                                       PetActionController &petActionsRef,
                                       AnimationController &animationsRef,
                                       Renderer &rendererRef)
    : config(configRef),
      petActions(petActionsRef),
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
    PetBehaviorStatValues state = readStats(petActions);
    initializePetBehaviorStats(config, state);
    writeStats(state, petActions);
}

bool PetBehaviorRuntime::advancePetDay()
{
    PetBehaviorStatValues state = readStats(petActions);
    applyPetBehaviorDailyChanges(config, state, dailyChangePauses);
    return petActions.commitPetDay(state.values, kPetBehaviorSlotCount);
}

PetBehaviorActionResult PetBehaviorRuntime::executeAction(uint8_t actionSlot)
{
    if (!hasAction(actionSlot))
        return PetBehaviorActionResult::Rejected;

    PetBehaviorStatValues state = readStats(petActions);
    PetBehaviorDailyChangePauses nextPauses = dailyChangePauses;
    PetBehaviorActionPlayback playback = {};
    if (!applyPetBehaviorAction(
            config, actionSlot, state, nextPauses, playback, arduinoRandomBelow))
        return PetBehaviorActionResult::Rejected;
    if (!writeStats(state, petActions))
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
    PetBehaviorStatValues state = readStats(petActions);
    if (!applyPetBehaviorGuessOutcome(config, outcome, state))
        return false;
    return writeStats(state, petActions);
}
#endif

const char *PetBehaviorRuntime::baseAnimation() const
{
    const PetBehaviorStatValues state = readStats(petActions);
    return resolvePetBehaviorBaseAnimation(config, state);
}
