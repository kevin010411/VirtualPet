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
    for (uint8_t slot = 0; slot < kPetBehaviorSlotCount; ++slot)
        state.values[slot] = petActions.customStat(slot);
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
    : config(configRef), petActions(petActionsRef), animations(animationsRef)
{
}

bool PetBehaviorRuntime::hasAction(uint8_t actionSlot) const
{
    return actionSlot < kPetBehaviorSlotCount && config.actions[actionSlot].active;
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
    applyPetBehaviorDailyChanges(config, state);
    return petActions.commitPetDay(state.values, kPetBehaviorSlotCount);
}

bool PetBehaviorRuntime::executeAction(uint8_t actionSlot)
{
    if (!hasAction(actionSlot))
        return false;

    PetBehaviorStatValues state = readStats(petActions);
    PetBehaviorActionPlayback playback = {};
    if (!applyPetBehaviorAction(config, actionSlot, state, playback))
        return false;
    if (!writeStats(state, petActions))
        return false;

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
