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

void writeStats(const PetBehaviorConfig &config,
                const PetBehaviorStatValues &state,
                PetActionController &petActions)
{
    for (uint8_t slot = 0; slot < kPetBehaviorSlotCount; ++slot)
    {
        if (config.stats[slot].active)
            petActions.setCustomStat(slot, state.values[slot]);
    }
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
    writeStats(config, state, petActions);
}

void PetBehaviorRuntime::applyDailyChanges()
{
    PetBehaviorStatValues state = readStats(petActions);
    applyPetBehaviorDailyChanges(config, state);
    writeStats(config, state, petActions);
}

bool PetBehaviorRuntime::executeAction(uint8_t actionSlot)
{
    if (!hasAction(actionSlot))
        return false;

    PetBehaviorStatValues state = readStats(petActions);
    PetBehaviorActionPlayback playback = {};
    if (!applyPetBehaviorAction(config, actionSlot, state, playback))
        return false;
    writeStats(config, state, petActions);

    if (!animations.queueRepeatedActionAnimation(
            playback.animation,
            playback.playbackCount,
            AnimationOwner::Command,
            AnimationPriority::High))
    {
        animations.showResourceError();
    }
    animations.markDirty();
    return true;
}

const char *PetBehaviorRuntime::baseAnimation() const
{
    const PetBehaviorStatValues state = readStats(petActions);
    return resolvePetBehaviorBaseAnimation(config, state);
}
