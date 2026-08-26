#include <assert.h>
#include <limits.h>
#include <string.h>

#include "pet_behavior/domain/PetBehaviorRuntimeRules.h"

namespace
{
PetBehaviorConfig behaviorConfig()
{
    PetBehaviorConfig config = {};
    config.stats[0].active = true;
    config.stats[0].initialValue = 10;
    config.stats[0].minValue = -20;
    config.stats[0].maxValue = 20;
    config.stats[0].dailyChange = -7;
    config.stats[3].active = true;
    config.stats[3].initialValue = 100;
    config.stats[3].minValue = 0;
    config.stats[3].maxValue = 100;
    config.stats[3].dailyChange = 50;
    config.statCount = 2;
    config.actions[2].active = true;
    strcpy(config.actions[2].animationPlayback.animation, "anim4");
    config.actions[2].animationPlayback.playbackCount = 5;
    config.actionCount = 1;
    return config;
}

void testInitialAndDailyChangesUseSparseSlotsAndClamp()
{
    const PetBehaviorConfig config = behaviorConfig();
    PetBehaviorStatValues state = {};
    PetBehaviorDailyChangePauses pauses = {};
    state.values[1] = 77;

    initializePetBehaviorStats(config, state);
    assert(state.values[0] == 10);
    assert(state.values[1] == 77);
    assert(state.values[3] == 100);

    applyPetBehaviorDailyChanges(config, state, pauses);
    assert(state.values[0] == 3);
    assert(state.values[3] == 100);
}

void testZeroAndCompoundEffectsExecuteWithoutRequirements()
{
    PetBehaviorConfig config = behaviorConfig();
    PetBehaviorStatValues state = {};
    PetBehaviorDailyChangePauses pauses = {};
    initializePetBehaviorStats(config, state);
    PetBehaviorActionPlayback playback = {};

    assert(applyPetBehaviorAction(config, 2, state, pauses, playback));
    assert(state.values[0] == 10);
    assert(strcmp(playback.animation, "anim4") == 0);
    assert(playback.playbackCount == 5);

    config.actionEffects[0] = {true, 2, 0, PetBehaviorActionEffectConfig::Operation::Change, INT16_MAX};
    config.actionEffects[1] = {true, 2, 3, PetBehaviorActionEffectConfig::Operation::Set, INT16_MIN};
    config.actionEffectCount = 2;
    assert(applyPetBehaviorAction(config, 2, state, pauses, playback));
    assert(state.values[0] == 20);
    assert(state.values[3] == 0);
}

void testInactiveOrInvalidActionsDoNotMutateState()
{
    PetBehaviorConfig config = behaviorConfig();
    PetBehaviorStatValues state = {};
    PetBehaviorDailyChangePauses pauses = {};
    initializePetBehaviorStats(config, state);
    const PetBehaviorStatValues before = state;
    PetBehaviorActionPlayback playback = {};

    assert(!applyPetBehaviorAction(config, 7, state, pauses, playback));
    assert(memcmp(&state, &before, sizeof(state)) == 0);

    config.actionEffects[0] = {true, 2, 7, PetBehaviorActionEffectConfig::Operation::Change, 5};
    config.actionEffectCount = 1;
    assert(!applyPetBehaviorAction(config, 2, state, pauses, playback));
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
