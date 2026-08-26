#include "pet_behavior/application/PetBehaviorRuntime.h"

#include "animation/application/AnimationController.h"
#include "animation/domain/Animation.h"
#include "pet/application/PetActionController.h"
#include "pet_behavior/domain/PetBehaviorRuntimeRules.h"

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
} // namespace

PetBehaviorRuntime::PetBehaviorRuntime(const PetBehaviorConfig &configRef,
                                       PetActionController &petActionsRef,
                                       AnimationController &animationsRef)
    : config(configRef),
      petActions(petActionsRef),
      animations(animationsRef),
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

bool PetBehaviorRuntime::executeAction(uint8_t actionSlot)
{
    if (!hasAction(actionSlot))
        return false;

    PetBehaviorStatValues state = readStats(petActions);
    PetBehaviorDailyChangePauses nextPauses = dailyChangePauses;
    PetBehaviorActionPlayback playback = {};
    if (!applyPetBehaviorAction(config, actionSlot, state, nextPauses, playback))
        return false;
    if (!writeStats(state, petActions))
        return false;
    dailyChangePauses = nextPauses;

    if (!animations.hasActionAnimation(playback.animation))
    {
        animations.showActionAnimationError();
    }
    else if (!animations.queueRepeatedActionAnimation(
            playback.animation,
            playback.playbackCount,
            AnimationOwner::Command,
            AnimationPriority::High))
    {
        return true;
    }
    else
    {
        animations.markDirty();
    }
    return true;
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
