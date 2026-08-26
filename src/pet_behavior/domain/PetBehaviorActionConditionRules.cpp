#include "pet_behavior/domain/PetBehaviorActionConditionRules.h"

#include <stddef.h>
#include <string.h>

namespace
{
struct ComparatorDescriptor
{
    const char *token;
    bool hasLowerBound;
    bool lowerInclusive;
    bool hasUpperBound;
    bool upperInclusive;
};

constexpr ComparatorDescriptor kComparatorDescriptors[] = {
    {"<", false, false, true, false},
    {"<=", false, false, true, true},
    {"=", true, true, true, true},
    {">=", true, true, false, false},
    {">", true, false, false, false},
};

constexpr size_t kComparatorCount =
    static_cast<size_t>(PetBehaviorActionConditionOperator::Count);
static_assert(sizeof(kComparatorDescriptors) / sizeof(kComparatorDescriptors[0]) == kComparatorCount,
              "Every conditional animation comparator needs one descriptor.");

const ComparatorDescriptor *descriptorFor(PetBehaviorActionConditionOperator comparison)
{
    const size_t index = static_cast<size_t>(comparison);
    return index < kComparatorCount ? &kComparatorDescriptors[index] : nullptr;
}
} // namespace

bool parsePetBehaviorActionConditionOperator(
    const char *token,
    PetBehaviorActionConditionOperator &comparison)
{
    if (token == nullptr)
        return false;
    for (size_t index = 0; index < kComparatorCount; ++index)
    {
        if (strcmp(token, kComparatorDescriptors[index].token) == 0)
        {
            comparison = static_cast<PetBehaviorActionConditionOperator>(index);
            return true;
        }
    }
    return false;
}

bool petBehaviorActionConditionInterval(
    PetBehaviorActionConditionOperator comparison,
    int32_t threshold,
    int64_t domainMinimum,
    int64_t domainMaximum,
    PetBehaviorActionConditionInterval &interval)
{
    const ComparatorDescriptor *descriptor = descriptorFor(comparison);
    if (descriptor == nullptr || domainMinimum > domainMaximum)
        return false;

    interval.minimum = descriptor->hasLowerBound
                           ? static_cast<int64_t>(threshold) + (descriptor->lowerInclusive ? 0 : 1)
                           : domainMinimum;
    interval.maximum = descriptor->hasUpperBound
                           ? static_cast<int64_t>(threshold) - (descriptor->upperInclusive ? 0 : 1)
                           : domainMaximum;
    if (interval.minimum < domainMinimum)
        interval.minimum = domainMinimum;
    if (interval.maximum > domainMaximum)
        interval.maximum = domainMaximum;
    return interval.minimum <= interval.maximum;
}

bool petBehaviorActionConditionMatches(
    PetBehaviorActionConditionOperator comparison,
    int32_t threshold,
    int64_t current)
{
    PetBehaviorActionConditionInterval interval = {};
    return petBehaviorActionConditionInterval(comparison, threshold, current, current, interval);
}
