#include "pet/PetStateClassifier.h"

#include "pet/PetBehaviorStatSlot.h"
#include "pet/RuntimeValueResolver.h"

ActivePetState PetStateClassifier::classify(const PetBehaviorConfig &config,
                                             const PetStatSnapshot &snapshot)
{
    RuntimeValueContext context = {};
    context.petStats = snapshot.customStats;
    context.activePetStatMask = activePetBehaviorStatMask(config);
    context.stageDays = snapshot.stage_days;
    context.speciesSlot = snapshot.speciesSlot;
    context.outfitSlot = snapshot.outfitSlot;

    for (uint8_t slot = 0; slot < config.petStateCount; ++slot)
    {
        const RuntimePetStateConfig &state = config.petStates[slot];
        if (matchesRuntimeRange(state.predicate, context))
        {
            ActivePetState active = {};
            active.slot = slot;
            active.idleAnimation = state.idleAnimation;
            active.isDefault = false;
            return active;
        }
    }

    ActivePetState fallback = {};
    fallback.slot = kDefaultPetStateSlot;
    fallback.idleAnimation = config.idleAnimation;
    fallback.isDefault = true;
    return fallback;
}
