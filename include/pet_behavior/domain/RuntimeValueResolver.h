#ifndef RUNTIME_VALUE_RESOLVER_H
#define RUNTIME_VALUE_RESOLVER_H

#include <stdint.h>

// Runtime Value IDs are the bounded, language-neutral scalar vocabulary used
// by every runtime predicate.  IDs 0..2 are reserved for the non-stat axes;
// projected Pet Stats occupy the fixed range beginning at 16.
using RuntimeValueId = uint16_t;
constexpr RuntimeValueId kRuntimeValueStageDays = 0;
constexpr RuntimeValueId kRuntimeValueSpeciesSlot = 1;
constexpr RuntimeValueId kRuntimeValueOutfitSlot = 2;
constexpr RuntimeValueId kRuntimeValuePetStatBase = 16;
constexpr uint8_t kRuntimeValuePetStatCapacity = 10;
constexpr RuntimeValueId kRuntimeValueIdMin = 0;
constexpr RuntimeValueId kRuntimeValueIdMax = 65534;

constexpr RuntimeValueId runtimeValueIdForPetStat(uint8_t slot)
{
    return static_cast<RuntimeValueId>(kRuntimeValuePetStatBase + slot);
}

constexpr bool isRuntimeValueIdKnown(RuntimeValueId valueId)
{
    return valueId == kRuntimeValueStageDays ||
           valueId == kRuntimeValueSpeciesSlot ||
           valueId == kRuntimeValueOutfitSlot ||
           (valueId >= kRuntimeValuePetStatBase &&
            valueId < kRuntimeValuePetStatBase + kRuntimeValuePetStatCapacity);
}

constexpr bool isRuntimeValueIdPetStat(RuntimeValueId valueId)
{
    return valueId >= kRuntimeValuePetStatBase &&
           valueId < kRuntimeValuePetStatBase + kRuntimeValuePetStatCapacity;
}

constexpr uint8_t runtimePetStatSlot(RuntimeValueId valueId)
{
    return static_cast<uint8_t>(valueId - kRuntimeValuePetStatBase);
}

struct RuntimeRangePredicate
{
    RuntimeValueId valueId = 0;
    int32_t minimum = 0;
    int32_t maximum = 0;
};

// Callers normalize their ownership model into one fixed-capacity value view;
// the resolver never depends on configuration or adapter types.
struct RuntimeValueContext
{
    const int16_t *petStats = nullptr;
    uint16_t activePetStatMask = 0;
    uint32_t stageDays = 0;
    uint8_t speciesSlot = 0;
    uint8_t outfitSlot = 0;
};

bool resolveRuntimeValue(RuntimeValueId valueId,
                         const RuntimeValueContext &context,
                         int32_t &value);

bool matchesRuntimeRange(const RuntimeRangePredicate &predicate,
                         const RuntimeValueContext &context);

bool isRuntimeRangeWellFormed(const RuntimeRangePredicate &predicate);

// Action, Status and Pet State predicates intentionally expose only
// stage_days and projected Pet Stats. Export and host tooling own domain
// clipping; firmware retains only executable-shape and capacity guards.
bool isRuntimeBehaviorRange(const RuntimeRangePredicate &predicate);

#endif // RUNTIME_VALUE_RESOLVER_H
