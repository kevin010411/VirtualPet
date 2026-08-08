#include <assert.h>
#include <limits.h>
#include <string.h>

#include "pet_behavior/domain/PetBehaviorRuntimeRules.h"

namespace
{
PetBehaviorConfig behaviorConfig()
{
    PetBehaviorConfig config = {};
    config.stats[0] = {true, 10, -20, 20, -7};
    config.stats[3] = {true, 100, 0, 100, 50};
    config.statCount = 2;
    config.actions[2].active = true;
    strcpy(config.actions[2].animation, "anim4");
    config.actions[2].playbackCount = 10;
    config.actionCount = 1;
    return config;
}

void testInitialAndDailyChangesUseSparseSlotsAndClamp()
{
    const PetBehaviorConfig config = behaviorConfig();
    PetBehaviorStatValues state = {};
    state.values[1] = 77;

    initializePetBehaviorStats(config, state);
    assert(state.values[0] == 10);
    assert(state.values[1] == 77);
    assert(state.values[3] == 100);

    applyPetBehaviorDailyChanges(config, state);
    assert(state.values[0] == 3);
    assert(state.values[3] == 100);
}

void testZeroAndCompoundEffectsExecuteWithoutRequirements()
{
    PetBehaviorConfig config = behaviorConfig();
    PetBehaviorStatValues state = {};
    initializePetBehaviorStats(config, state);
    PetBehaviorActionPlayback playback = {};

    assert(applyPetBehaviorAction(config, 2, state, playback));
    assert(state.values[0] == 10);
    assert(strcmp(playback.animation, "anim4") == 0);
    assert(playback.playbackCount == 10);

    config.actionEffects[0] = {true, 2, 0, INT16_MAX};
    config.actionEffects[1] = {true, 2, 3, INT16_MIN};
    config.actionEffectCount = 2;
    assert(applyPetBehaviorAction(config, 2, state, playback));
    assert(state.values[0] == 20);
    assert(state.values[3] == 0);
}

void testInactiveOrInvalidActionsDoNotMutateState()
{
    PetBehaviorConfig config = behaviorConfig();
    PetBehaviorStatValues state = {};
    initializePetBehaviorStats(config, state);
    const PetBehaviorStatValues before = state;
    PetBehaviorActionPlayback playback = {};

    assert(!applyPetBehaviorAction(config, 7, state, playback));
    assert(memcmp(&state, &before, sizeof(state)) == 0);

    config.actionEffects[0] = {true, 2, 7, 5};
    config.actionEffectCount = 1;
    assert(!applyPetBehaviorAction(config, 2, state, playback));
    assert(memcmp(&state, &before, sizeof(state)) == 0);
}
} // namespace

int main()
{
    testInitialAndDailyChangesUseSparseSlotsAndClamp();
    testZeroAndCompoundEffectsExecuteWithoutRequirements();
    testInactiveOrInvalidActionsDoNotMutateState();
    return 0;
}
