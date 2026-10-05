#include "pet_behavior/domain/PetBehaviorRuntimeRules.h"

#include "pet_behavior/domain/PetBehaviorStatSlot.h"

namespace
{
int16_t clampedChange(int16_t current, int16_t delta, int16_t minimum, int16_t maximum)
{
    const int32_t next = static_cast<int32_t>(current) + static_cast<int32_t>(delta);
    if (next < minimum)
        return minimum;
    if (next > maximum)
        return maximum;
    return static_cast<int16_t>(next);
}

bool applyEffect(const PetBehaviorConfig &config,
                 uint8_t statSlot,
                 PetBehaviorEffectOperation operation,
                 int16_t value,
                 PetBehaviorStatValues &state,
                 bool *affectedSlots)
{
    if (statSlot >= kPetBehaviorSlotCount || !config.stats[statSlot].active || affectedSlots[statSlot])
        return false;
    affectedSlots[statSlot] = true;
    const PetBehaviorStatConfig &stat = config.stats[statSlot];
    state.values[statSlot] = operation == PetBehaviorEffectOperation::Set
                                 ? clampedChange(0, value, stat.minValue, stat.maxValue)
                                 : clampedChange(state.values[statSlot], value, stat.minValue, stat.maxValue);
    return true;
}

// Check both published counts and compiled storage before dereferencing a range.
bool validRange(uint8_t first, uint8_t count, uint16_t total, uint16_t capacity)
{
    return total <= capacity && static_cast<uint16_t>(first) + count <= total;
}

const PetBehaviorActionOutcomeConfig *selectOutcome(
    const PetBehaviorConfig &config, const PetBehaviorActionConfig &action,
    PetBehaviorRandomBoundedSource randomSource)
{
    if (!validRange(action.firstOutcome, action.outcomeCount,
                    config.actionOutcomeCount, kMaxPetBehaviorActionOutcomes))
        return nullptr;
    const auto *outcomes = config.actionOutcomes + action.firstOutcome;
    if (action.mode != PetBehaviorActionMode::RandomOutcome)
        return action.outcomeCount == 1 ? outcomes : nullptr;
    if (action.outcomeCount < kMinPetBehaviorRandomOutcomesPerAction ||
        action.outcomeCount > kMaxPetBehaviorRandomOutcomesPerAction || randomSource == nullptr)
        return nullptr;

    uint16_t totalWeight = 0;
    for (uint8_t index = 0; index < action.outcomeCount; ++index)
        totalWeight += outcomes[index].weight;
    if (totalWeight == 0)
        return nullptr;
    const uint16_t selectedWeight = randomSource(totalWeight);
    if (selectedWeight >= totalWeight)
        return nullptr;
    uint16_t coveredWeight = 0;
    for (uint8_t index = 0; index < action.outcomeCount; ++index)
    {
        coveredWeight += outcomes[index].weight;
        if (selectedWeight < coveredWeight)
            return &outcomes[index];
    }
    return nullptr;
}

bool selectPlayback(const PetBehaviorConfig &config,
                    const PetBehaviorActionConfig &action,
                    const PetBehaviorActionOutcomeConfig &outcome,
                    const PetBehaviorStatValues &state,
                    PetBehaviorActionPlayback &playback)
{
    if (action.mode == PetBehaviorActionMode::Standard ||
        action.mode == PetBehaviorActionMode::RandomOutcome)
    {
        playback = outcome.animationPlayback;
        return true;
    }
    if (action.mode != PetBehaviorActionMode::ConditionalAnimation ||
        action.conditionCount > kMaxPetBehaviorActionConditionsPerAction ||
        !validRange(action.firstCondition, action.conditionCount,
                    config.actionConditionCount, kMaxPetBehaviorActionConditions))
        return false;

    RuntimeValueContext context = {};
    context.petStats = state.values;
    context.activePetStatMask = activePetBehaviorStatMask(config);
    context.stageDays = state.stageDays;
    const PetBehaviorActionConditionConfig *selected = nullptr;
    for (uint8_t index = 0; index < action.conditionCount; ++index)
    {
        const auto &condition = config.actionConditions[action.firstCondition + index];
        if (matchesRuntimeRange(condition.predicate, context) &&
            (selected == nullptr || condition.priority < selected->priority))
            selected = &condition;
    }
    if (selected != nullptr)
    {
        playback = selected->animationPlayback;
        return true;
    }
    if (!action.hasFallbackAnimation)
        return false;
    playback = outcome.animationPlayback;
    return true;
}

bool applyOutcomeEffects(const PetBehaviorConfig &config,
                         const PetBehaviorActionOutcomeConfig &outcome,
                         PetBehaviorStatValues &state, bool *affectedSlots)
{
    if (outcome.effectCount > kPetBehaviorSlotCount ||
        !validRange(outcome.firstEffect, outcome.effectCount,
                    config.actionEffectCount, kMaxPetBehaviorActionEffects))
        return false;
    for (uint8_t index = 0; index < outcome.effectCount; ++index)
    {
        const auto &effect = config.actionEffects[outcome.firstEffect + index];
        if (!applyEffect(config, effect.statSlot, effect.operation, effect.value, state, affectedSlots))
            return false;
    }
    return true;
}
} // namespace

void initializePetBehaviorStats(const PetBehaviorConfig &config, PetBehaviorStatValues &state)
{
    for (uint8_t slot = 0; slot < kPetBehaviorSlotCount; ++slot)
    {
        if (config.stats[slot].active)
            state.values[slot] = config.stats[slot].initialValue;
    }
}

void applyPetBehaviorDailyChanges(const PetBehaviorConfig &config,
                                  PetBehaviorStatValues &state,
                                  PetBehaviorDailyChangePauses &pauses)
{
    for (uint8_t slot = 0; slot < kPetBehaviorSlotCount; ++slot)
    {
        const PetBehaviorStatConfig &stat = config.stats[slot];
        if (!stat.active)
            continue;
        if (pauses.remainingDays[slot] > 0)
        {
            --pauses.remainingDays[slot];
            continue;
        }
        state.values[slot] = clampedChange(
            state.values[slot], stat.dailyChange, stat.minValue, stat.maxValue);
    }
}

bool applyPetBehaviorAction(const PetBehaviorConfig &config,
                            uint8_t actionSlot,
                            PetBehaviorStatValues &state,
                            PetBehaviorDailyChangePauses &pauses,
                            PetBehaviorActionPlayback &playback,
                            PetBehaviorRandomBoundedSource randomSource)
{
    playback = {};
    if (actionSlot >= kMaxPetBehaviorActions || !config.actions[actionSlot].active)
        return false;

    const PetBehaviorActionConfig &action = config.actions[actionSlot];
    const auto *outcome = selectOutcome(config, action, randomSource);
    if (outcome == nullptr || !selectPlayback(config, action, *outcome, state, playback))
        return false;

    // Resolve conditions against the original values, then apply the selected
    // Outcome to a temporary state. Publish values and pauses only on success.
    PetBehaviorStatValues next = state;
    bool affectedSlots[kPetBehaviorSlotCount] = {};
    if (!applyOutcomeEffects(config, *outcome, next, affectedSlots))
        return false;

    for (uint8_t slot = 0; slot < kPetBehaviorSlotCount; ++slot)
    {
        if (affectedSlots[slot] && next.values[slot] != state.values[slot] &&
            pauses.remainingDays[slot] < action.suspendDailyChangeDays)
        {
            pauses.remainingDays[slot] = action.suspendDailyChangeDays;
        }
    }
    state = next;
    return true;
}

#if ENABLE_GUESS_GAME
bool applyPetBehaviorGuessOutcome(const PetBehaviorConfig &config,
                                  PetBehaviorGuessOutcome outcome,
                                  PetBehaviorStatValues &state)
{
    PetBehaviorStatValues next = state;
    bool affectedSlots[kPetBehaviorSlotCount] = {};
    for (uint8_t index = 0; index < config.guessEffectCount; ++index)
    {
        const PetBehaviorGuessEffectConfig &effect = config.guessEffects[index];
        if (!effect.active || effect.outcome != outcome)
            continue;
        if (!applyEffect(config, effect.statSlot, effect.operation, effect.value, next, affectedSlots))
            return false;
    }
    state = next;
    return true;
}
#endif
