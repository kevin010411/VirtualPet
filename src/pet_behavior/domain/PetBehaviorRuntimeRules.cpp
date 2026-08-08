#include "pet_behavior/domain/PetBehaviorRuntimeRules.h"

#include <string.h>

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
} // namespace

void initializePetBehaviorStats(const PetBehaviorConfig &config, PetBehaviorStatValues &state)
{
    for (uint8_t slot = 0; slot < kPetBehaviorSlotCount; ++slot)
    {
        if (config.stats[slot].active)
            state.values[slot] = config.stats[slot].initialValue;
    }
}

void applyPetBehaviorDailyChanges(const PetBehaviorConfig &config, PetBehaviorStatValues &state)
{
    for (uint8_t slot = 0; slot < kPetBehaviorSlotCount; ++slot)
    {
        const PetBehaviorStatConfig &stat = config.stats[slot];
        if (stat.active)
            state.values[slot] = clampedChange(
                state.values[slot], stat.dailyChange, stat.minValue, stat.maxValue);
    }
}

bool applyPetBehaviorAction(const PetBehaviorConfig &config,
                            uint8_t actionSlot,
                            PetBehaviorStatValues &state,
                            PetBehaviorActionPlayback &playback)
{
    playback = {};
    if (actionSlot >= kPetBehaviorSlotCount || !config.actions[actionSlot].active)
        return false;

    PetBehaviorStatValues next = state;
    for (uint8_t index = 0; index < config.actionEffectCount; ++index)
    {
        const PetBehaviorActionEffectConfig &effect = config.actionEffects[index];
        if (!effect.active || effect.actionSlot != actionSlot)
            continue;
        if (effect.statSlot >= kPetBehaviorSlotCount || !config.stats[effect.statSlot].active)
            return false;

        const PetBehaviorStatConfig &stat = config.stats[effect.statSlot];
        next.values[effect.statSlot] = clampedChange(
            next.values[effect.statSlot], effect.delta, stat.minValue, stat.maxValue);
    }

    state = next;
    const PetBehaviorActionConfig &action = config.actions[actionSlot];
    strncpy(playback.animation, action.animation, sizeof(playback.animation) - 1);
    playback.animation[sizeof(playback.animation) - 1] = '\0';
    playback.playbackCount = action.playbackCount;
    return true;
}

const char *resolvePetBehaviorBaseAnimation(const PetBehaviorConfig &config,
                                            const PetBehaviorStatValues &state)
{
    for (uint8_t index = 0; index < config.healthStatusCount; ++index)
    {
        const PetBehaviorHealthStatusConfig &status = config.healthStatuses[index];
        if (status.active && status.statSlot < kPetBehaviorSlotCount &&
            state.values[status.statSlot] < status.threshold)
        {
            return status.animation;
        }
    }
    return config.idleAnimation;
}
