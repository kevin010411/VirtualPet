#include "pet/PetBehaviorRuntimeRules.h"

#include "pet/PetBehaviorStatSlot.h"
#include "pet/Pet.h"

namespace
{
uint32_t clampedChange(uint32_t current, int64_t delta, uint32_t minimum, uint32_t maximum)
{
    const int64_t next = static_cast<int64_t>(current) + static_cast<int64_t>(delta);
    if (next < minimum)
        return minimum;
    if (next > maximum)
        return maximum;
    return static_cast<uint32_t>(next);
}

bool applyEffect(const PetBehaviorConfig &config,
                 uint8_t statSlot,
                 PetBehaviorEffectOperation operation,
                 int64_t value,
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
bool validRange(uint16_t first, uint8_t count, uint16_t total, uint16_t capacity)
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
        action.outcomeCount > kMaxPetBehaviorLotteryOutcomesPerAction || randomSource == nullptr)
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

    const auto context = makeRuntimeValueContext(state.values,
        activePetBehaviorStatMask(config), state.stageDays);
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
    playback = action.hasFallbackAnimation ? outcome.animationPlayback : PetBehaviorActionPlayback{};
    return true;
}

bool applyOutcomeEffects(const PetBehaviorConfig &config,
                         const PetBehaviorActionOutcomeConfig &outcome,
                         PetBehaviorStatValues &state, bool *affectedSlots,
                         PetBehaviorEffectReader reader, void *context)
{
    if (outcome.effectCount > kPetBehaviorSlotCount ||
        !validRange(outcome.firstEffect, outcome.effectCount,
                    config.actionEffectCount, kMaxPetBehaviorActionEffects))
        return false;
    for (uint8_t index = 0; index < outcome.effectCount; ++index)
    {
        PetBehaviorActionEffectConfig effect = {};
        const uint16_t row = outcome.firstEffect + index;
        if (config.actionEffectsStreamed)
        {
            if (reader == nullptr || !reader(context, row, effect)) return false;
        }
        else
        {
            if (row >= kStoredPetBehaviorActionEffects) return false;
            effect = config.actionEffects[row];
        }
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
                                  PetBehaviorDailyChangePauses &pauses, uint32_t days)
{
    for (uint8_t slot = 0; slot < kPetBehaviorSlotCount; ++slot)
    {
        const PetBehaviorStatConfig &stat = config.stats[slot];
        if (!stat.active)
            continue;
        const uint32_t paused = days < pauses.remainingDays[slot] ? days : pauses.remainingDays[slot];
        pauses.remainingDays[slot] -= paused;
        uint32_t activeDays = days - paused;
        const int64_t delta = stat.dailyChange;
        if (delta == 0 || activeDays == 0) continue;
        const int64_t value = state.values[slot];
        const int64_t gap = delta > 0 ? stat.maxValue - value : value - stat.minValue;
        const uint32_t magnitude = delta > 0 ? delta : -delta;
        const int64_t untilClamp = (gap + magnitude - 1) / magnitude;
        if (activeDays > untilClamp) activeDays = untilClamp;
        const int64_t next = value + delta * static_cast<int64_t>(activeDays);
        state.values[slot] = next < stat.minValue ? stat.minValue :
                            next > stat.maxValue ? stat.maxValue : static_cast<uint32_t>(next);
    }
}

uint32_t petBehaviorRangeChangeDays(const PetBehaviorConfig &config,
                                   const PetBehaviorStatValues &state,
                                   const PetBehaviorDailyChangePauses &pauses,
                                   const RuntimeRangePredicate &predicate, uint32_t limit)
{
    int64_t value, delta, low, high;
    if (predicate.valueId == kRuntimeValueStageDays)
    {
        value = state.stageDays;
        delta = 1;
        low = 0;
        high = Pet::kStageDaysMax;
    }
    else if (isRuntimeValueIdPetStat(predicate.valueId))
    {
        const uint8_t slot = runtimePetStatSlot(predicate.valueId);
        if (slot >= kPetBehaviorSlotCount || !config.stats[slot].active || pauses.remainingDays[slot])
            return limit;
        const auto &stat = config.stats[slot];
        value = state.values[slot];
        delta = stat.dailyChange;
        low = stat.minValue;
        high = stat.maxValue;
    }
    else return limit; // Species, Outfit and action counts do not drift with time.

    if (delta == 0 || predicate.minimum > high || predicate.maximum < low) return limit;
    const int64_t minimum = predicate.minimum < low ? low : predicate.minimum;
    const int64_t maximum = predicate.maximum > high ? high : predicate.maximum;
    int64_t days;
    if (delta > 0)
    {
        if (value < minimum) days = (minimum - value + delta - 1) / delta;
        else if (value <= maximum && maximum < high) days = (maximum - value) / delta + 1;
        else return limit;
    }
    else
    {
        const int64_t magnitude = -delta;
        if (value > maximum) days = (value - maximum + magnitude - 1) / magnitude;
        else if (value >= minimum && minimum > low) days = (value - minimum) / magnitude + 1;
        else return limit;
    }
    return days < limit ? days : limit;
}

uint32_t petBehaviorCatchUpDays(const PetBehaviorConfig &config,
                              const PetBehaviorStatValues &state,
                              const PetBehaviorDailyChangePauses &pauses, uint32_t limit)
{
    for (uint8_t slot = 0; slot < kPetBehaviorSlotCount; ++slot)
    {
        if (!config.stats[slot].active) continue;
        const uint32_t paused = pauses.remainingDays[slot];
        if (paused && paused < limit) limit = paused;
#if ENABLE_DEATH
        if (config.deathEnabled && (config.deathStatMask & (1U << slot)))
            limit = petBehaviorRangeChangeDays(config, state, pauses,
                {runtimeValueIdForPetStat(slot), INT32_MIN, 0}, limit);
#endif
    }
    for (uint8_t index = 0; index < config.petStateCount; ++index)
        limit = petBehaviorRangeChangeDays(config, state, pauses, config.petStates[index].predicate, limit);
    return limit;
}

bool applyPetBehaviorAction(const PetBehaviorConfig &config,
                            uint8_t actionSlot,
                            PetBehaviorStatValues &state,
                            PetBehaviorDailyChangePauses &pauses,
                            PetBehaviorActionPlayback &playback,
                            PetBehaviorRandomBoundedSource randomSource,
                            PetBehaviorEffectReader effectReader, void *effectContext)
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
    if (!applyOutcomeEffects(config, *outcome, next, affectedSlots, effectReader, effectContext))
        return false;

    for (uint8_t slot = 0; slot < kPetBehaviorSlotCount; ++slot)
    {
        if (affectedSlots[slot] &&
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

#if ENABLE_INSTRUCTION_RESPONSE
bool applyPetBehaviorInstructionResults(const PetBehaviorConfig &config,
                                        uint32_t correctMask, uint8_t count,
                                        PetBehaviorStatValues &state)
{
    PetBehaviorStatValues next = state;
    for (uint8_t round = 0; round < count; ++round)
    {
        const uint8_t group = (correctMask & (1UL << round)) ? 0 : 1;
        for (uint8_t index = 0; index < config.instructionEffectCount; ++index)
        {
            const auto &effect = config.instructionEffects[index];
            if (effect.group != group) continue;
            const auto &stat = config.stats[effect.statSlot];
            next.values[effect.statSlot] = clampedChange(
                effect.operation == PetBehaviorEffectOperation::Set ? 0 : next.values[effect.statSlot],
                effect.value, stat.minValue, stat.maxValue);
        }
    }
    state = next;
    return true;
}
#endif
