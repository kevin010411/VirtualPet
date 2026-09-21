#include "pet_behavior/domain/RuntimeValueResolver.h"

#include <limits.h>
#include "pet_behavior/domain/PetBehaviorStatSlot.h"
#include "pet_behavior/domain/PetBehaviorTypes.h"

bool resolveRuntimeValue(RuntimeValueId valueId,
                         const RuntimeValueContext &context,
                         int32_t &value)
{
    value = 0;
    if (!isRuntimeValueIdKnown(valueId))
        return false;

    if (isRuntimeValueIdPetStat(valueId))
    {
        const uint8_t slot = runtimePetStatSlot(valueId);
        if (context.petStats == nullptr || slot >= context.petStatCapacity ||
            slot >= kPetBehaviorSlotCount ||
            (context.behaviorConfig == nullptr && context.activeStatSlots == nullptr) ||
            (context.behaviorConfig != nullptr && !context.behaviorConfig->stats[slot].active) ||
            (context.activeStatSlots != nullptr && !context.activeStatSlots->contains(slot)))
            return false;
        value = context.petStats[slot];
        return true;
    }

    switch (valueId)
    {
    case kRuntimeValueStageDays:
        value = context.stageDays > INT32_MAX ? INT32_MAX :
                static_cast<int32_t>(context.stageDays);
        return context.stageDays <= INT32_MAX;
    case kRuntimeValueSpeciesSlot:
        if (context.speciesSlot == 0)
            return false;
        value = context.speciesSlot;
        return true;
    case kRuntimeValueOutfitSlot:
        if (context.outfitSlot == 0)
            return false;
        value = context.outfitSlot;
        return true;
    default:
        return false;
    }
}

bool matchesRuntimeRange(const RuntimeRangePredicate &predicate,
                         const RuntimeValueContext &context)
{
    if (predicate.minimum > predicate.maximum)
        return false;
    int32_t value = 0;
    return resolveRuntimeValue(predicate.valueId, context, value) &&
           value >= predicate.minimum && value <= predicate.maximum;
}

bool runtimeRangeWithinCompiledDomain(const RuntimeRangePredicate &predicate,
                                      const PetBehaviorConfig &config)
{
    if (predicate.minimum > predicate.maximum)
        return false;
    if (predicate.valueId == kRuntimeValueStageDays)
        return predicate.minimum >= 0 && predicate.maximum <= 3650;
    if (!isRuntimeValueIdPetStat(predicate.valueId))
        return false;
    const uint8_t slot = runtimePetStatSlot(predicate.valueId);
    return slot < config.statCount && config.stats[slot].active &&
           predicate.minimum >= config.stats[slot].minValue &&
           predicate.maximum <= config.stats[slot].maxValue;
}
