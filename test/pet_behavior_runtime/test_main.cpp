#include <assert.h>
#include <limits.h>
#include <string.h>
#include "pet/PetBehaviorRuntimeRules.h"

namespace
{
using Operation = PetBehaviorEffectOperation;
uint16_t randomValue;
uint16_t randomBound;
uint16_t deterministicRandom(uint16_t bound)
{
    randomBound = bound;
    return randomValue;
}

PetBehaviorConfig behaviorConfig()
{
    PetBehaviorConfig config = {};
    config.stats[0] = {true, 10, -20, 20, -7};
    config.stats[3] = {true, 100, 0, 100, 50};
    config.statCount = 2;
    config.actions[2].active = true;
    config.actions[2].outcomeCount = 1;
    config.actionOutcomes[0].animationPlayback = {{1, 1, 4}, 5};
    config.actionOutcomeCount = 1;
    config.actionCount = 1;
    return config;
}

void assertRejected(const PetBehaviorConfig &config, uint8_t actionSlot = 2)
{
    PetBehaviorStatValues state = {};
    initializePetBehaviorStats(config, state);
    const PetBehaviorStatValues before = state;
    PetBehaviorDailyChangePauses pauses = {};
    pauses.remainingDays[0] = 8;
    const auto beforePauses = pauses;
    PetBehaviorActionPlayback playback = {};
    assert(!applyPetBehaviorAction(config, actionSlot, state, pauses, playback, deterministicRandom));
    assert(memcmp(&state, &before, sizeof(state)) == 0);
    assert(memcmp(&pauses, &beforePauses, sizeof(pauses)) == 0);
}

void testDailyChangesAndSparseStats()
{
    const auto config = behaviorConfig();
    PetBehaviorStatValues state = {};
    PetBehaviorDailyChangePauses pauses = {};
    state.values[1] = 77;
    initializePetBehaviorStats(config, state);
    assert(state.values[0] == 10 && state.values[1] == 77 && state.values[3] == 100);
    applyPetBehaviorDailyChanges(config, state, pauses);
    assert(state.values[0] == 3 && state.values[3] == 100);
}

void testAnimationOnlyAndOwnedEffects()
{
    auto config = behaviorConfig();
    PetBehaviorStatValues state = {};
    PetBehaviorDailyChangePauses pauses = {};
    initializePetBehaviorStats(config, state);
    PetBehaviorActionPlayback playback = {};
    assert(applyPetBehaviorAction(config, 2, state, pauses, playback));
    assert(state.values[0] == 10 && playback.animation.animationId == 4 && playback.playbackCount == 5);

    // Invalid effects outside the selected Outcome must never be visited.
    config.actionEffects[0] = {255, Operation::Change, 1};
    config.actionEffects[1] = {0, Operation::Change, INT16_MAX};
    config.actionEffects[2] = {3, Operation::Set, INT16_MIN};
    config.actionEffects[3] = {255, Operation::Change, 1};
    config.actionEffectCount = 4;
    config.actionOutcomes[0].firstEffect = 1;
    config.actionOutcomes[0].effectCount = 2;
    config.actions[2].suspendDailyChangeDays = 3;
    pauses.remainingDays[0] = 8;
    assert(applyPetBehaviorAction(config, 2, state, pauses, playback));
    assert(state.values[0] == 20 && state.values[3] == 0);
    assert(pauses.remainingDays[0] == 8 && pauses.remainingDays[3] == 3);
    playback = {}; // Playback failure after commit cannot undo the effects.
    applyPetBehaviorDailyChanges(config, state, pauses);
    assert(state.values[0] == 20 && state.values[3] == 0);
    assert(pauses.remainingDays[0] == 7 && pauses.remainingDays[3] == 2);

    // A clamped/no-op effect must not restart a suspension.
    config.actionOutcomes[0].effectCount = 1;
    pauses = {};
    assert(applyPetBehaviorAction(config, 2, state, pauses, playback));
    assert(pauses.remainingDays[0] == 0);
}

void testRejectedEffectsAreAtomic()
{
    auto config = behaviorConfig();
    assertRejected(config, 7);
    assertRejected(config, 255);
    config.actionEffects[0] = {0, Operation::Change, 5};
    config.actionEffects[1] = {255, Operation::Change, 1};
    config.actionEffectCount = 2;
    config.actionOutcomes[0].effectCount = 2;
    config.actions[2].suspendDailyChangeDays = 255;
    assertRejected(config);
    config.actionEffects[1].statSlot = 0; // Duplicate Stat after an otherwise valid effect.
    assertRejected(config);
    config.actionEffects[1].statSlot = 1; // Inactive Stat.
    assertRejected(config);
    config.actionOutcomes[0].firstEffect = 2;
    assertRejected(config);
    config.actionOutcomes[0].firstEffect = 255;
    assertRejected(config);
    config.actionOutcomes[0].firstEffect = 0;
    config.actionEffectCount = 255;
    assertRejected(config);
    config.actions[2].firstOutcome = 255;
    assertRejected(config);
    config.actions[2].firstOutcome = 0;
    config.actions[2].outcomeCount = 0;
    assertRejected(config);
}

void testConditionalUsesOwnedRangePriorityAndPreEffectValues()
{
    auto config = behaviorConfig();
    auto &action = config.actions[2];
    action.mode = PetBehaviorActionMode::ConditionalAnimation;
    action.firstCondition = 1;
    action.conditionCount = 2;
    config.actionConditionCount = 4;
    config.actionConditions[0] = {{runtimeValueIdForPetStat(0), -20, 20}, {{1, 1, 9}, 1}, 0};
    config.actionConditions[1] = {{runtimeValueIdForPetStat(0), 10, 10}, {{1, 1, 6}, 2}, 5};
    config.actionConditions[2] = {{runtimeValueIdForPetStat(0), 10, 10}, {{1, 1, 7}, 3}, 2};
    config.actionConditions[3] = config.actionConditions[0];
    config.actionEffects[0] = {0, Operation::Set, -5};
    config.actionEffectCount = 1;
    config.actionOutcomes[0].effectCount = 1;
    PetBehaviorStatValues state = {};
    initializePetBehaviorStats(config, state);
    PetBehaviorDailyChangePauses pauses = {};
    PetBehaviorActionPlayback playback = {};
    assert(applyPetBehaviorAction(config, 2, state, pauses, playback));
    assert(playback.animation.animationId == 7 && playback.playbackCount == 3);
    assert(state.values[0] == -5);
    assert(!applyPetBehaviorAction(config, 2, state, pauses, playback));
    action.hasFallbackAnimation = true;
    assert(applyPetBehaviorAction(config, 2, state, pauses, playback));
    assert(playback.animation.animationId == 4);
    action.firstCondition = 255;
    assertRejected(config);
}

void testWeightedOutcomeBoundariesAndEffects()
{
    auto config = behaviorConfig();
    auto &action = config.actions[2];
    action.mode = PetBehaviorActionMode::RandomOutcome;
    action.firstOutcome = 1;
    action.outcomeCount = 3;
    config.actionOutcomeCount = 4;
    config.actionOutcomes[0].weight = 100; // Another Action's Outcome.
    for (uint8_t index = 0; index < 3; ++index)
    {
        auto &outcome = config.actionOutcomes[index + 1];
        outcome.animationPlayback = {{1, 1, static_cast<uint16_t>(10 + index)}, 1};
        outcome.weight = static_cast<uint8_t>((index + 1) * 10);
        outcome.firstEffect = index;
        outcome.effectCount = 1;
        config.actionEffects[index] = {0, Operation::Set, static_cast<int16_t>(index)};
    }
    config.actionEffectCount = 3;
    const uint16_t samples[] = {0, 9, 10, 29, 30, 59};
    for (uint8_t index = 0; index < 6; ++index)
    {
        randomValue = samples[index];
        PetBehaviorStatValues state = {};
        PetBehaviorDailyChangePauses pauses = {};
        PetBehaviorActionPlayback playback = {};
        assert(applyPetBehaviorAction(config, 2, state, pauses, playback, deterministicRandom));
        assert(randomBound == 60);
        assert(playback.animation.animationId == 10 + index / 2);
        assert(state.values[0] == index / 2);
    }
    randomValue = 60;
    assertRejected(config);
    randomValue = 0;
    for (uint8_t index = 1; index < 4; ++index)
        config.actionOutcomes[index].weight = 0;
    assertRejected(config);
}
} // namespace

int main()
{
    testDailyChangesAndSparseStats();
    testAnimationOnlyAndOwnedEffects();
    testRejectedEffectsAreAtomic();
    testConditionalUsesOwnedRangePriorityAndPreEffectValues();
    testWeightedOutcomeBoundariesAndEffects();
    return 0;
}