#include "pet_behavior/domain/PetBehaviorRuntimeRules.h"

#include "pet_behavior/domain/PetBehaviorActionConditionRules.h"

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

bool conditionMatches(const PetBehaviorActionConditionConfig &condition,
                      const PetBehaviorStatValues &state)
{
    int64_t current = state.stageDays;
    if (condition.source == PetBehaviorActionConditionSource::PetStat)
    {
        if (condition.statSlot >= kPetBehaviorSlotCount)
            return false;
        current = state.values[condition.statSlot];
    }

    return petBehaviorActionConditionMatches(condition.comparison, condition.threshold, current);
}

bool selectActionPlayback(const PetBehaviorConfig &config,
                          uint8_t actionSlot,
                          const PetBehaviorStatValues &state,
                          PetBehaviorActionPlayback &playback)
{
    const PetBehaviorActionConfig &action = config.actions[actionSlot];
    if (action.mode == PetBehaviorActionMode::Standard)
    {
        playback = action.animationPlayback;
        return true;
    }

    const PetBehaviorActionConditionConfig *selected = nullptr;
    for (uint8_t index = 0; index < config.actionConditionCount; ++index)
    {
        const PetBehaviorActionConditionConfig &condition = config.actionConditions[index];
        if (!condition.active || condition.actionSlot != actionSlot ||
            !conditionMatches(condition, state))
            continue;
        if (selected == nullptr || condition.priority < selected->priority)
            selected = &condition;
    }
    if (selected != nullptr)
    {
        playback = selected->animationPlayback;
        return true;
    }
    if (!action.hasFallbackAnimation)
        return false;
    playback = action.animationPlayback;
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
                            PetBehaviorActionPlayback &playback)
{
    playback = {};
    if (actionSlot >= kMaxPetBehaviorActions || !config.actions[actionSlot].active)
        return false;

    if (!selectActionPlayback(config, actionSlot, state, playback))
        return false;

    PetBehaviorStatValues next = state;
    bool affectedSlots[kPetBehaviorSlotCount] = {};
    for (uint8_t index = 0; index < config.actionEffectCount; ++index)
    {
        const PetBehaviorActionEffectConfig &effect = config.actionEffects[index];
        if (!effect.active || effect.actionSlot != actionSlot)
            continue;
        if (!applyEffect(config, effect.statSlot, effect.operation, effect.value, next, affectedSlots))
            return false;
    }

    const PetBehaviorActionConfig &action = config.actions[actionSlot];
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

const char *resolvePetBehaviorBaseAnimation(const PetBehaviorConfig &config,
                                            const PetBehaviorStatValues &state)
{
    for (uint8_t index = 0; index < config.idleTriggerCount; ++index)
    {
        const PetBehaviorIdleTriggerConfig &trigger = config.idleTriggers[index];
        if (!trigger.active || trigger.statSlot >= kPetBehaviorSlotCount)
            continue;
        const int16_t value = state.values[trigger.statSlot];
        const bool active = trigger.comparison == PetBehaviorIdleTriggerOperator::LessThan ?
                                value < trigger.threshold :
                                value > trigger.threshold;
        if (active)
        {
            return trigger.animation;
        }
    }
    return config.idleAnimation;
}
