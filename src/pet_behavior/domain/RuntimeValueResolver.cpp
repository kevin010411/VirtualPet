#include "pet_behavior/domain/RuntimeValueResolver.h"

#include <limits.h>
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
        if (context.petStats == nullptr || slot >= kPetBehaviorSlotCount ||
            (context.activePetStatMask & static_cast<uint16_t>(1U << slot)) == 0)
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
    if (!isRuntimeRangeWellFormed(predicate))
        return false;
    int32_t value = 0;
    return resolveRuntimeValue(predicate.valueId, context, value) &&
           value >= predicate.minimum && value <= predicate.maximum;
}

bool isRuntimeRangeWellFormed(const RuntimeRangePredicate &predicate)
{
    return isRuntimeValueIdKnown(predicate.valueId) &&
           predicate.minimum <= predicate.maximum;
}

bool isRuntimeBehaviorRange(const RuntimeRangePredicate &predicate)
{
    return isRuntimeRangeWellFormed(predicate) &&
           (predicate.valueId == kRuntimeValueStageDays ||
            isRuntimeValueIdPetStat(predicate.valueId));
}
