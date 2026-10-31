#include "controller/StatusSetContract.h"

#include "pet/PetStateClassifier.h"

namespace
{
uint8_t levelForValue(int64_t value, int64_t minValue, int64_t maxValue, uint8_t levels)
{
    if (levels <= 1 || value <= minValue)
        return 0;
    if (value >= maxValue)
        return static_cast<uint8_t>(levels - 1);
    const int64_t valueCount = static_cast<int64_t>(maxValue) - minValue + 1;
    const int64_t offset = static_cast<int64_t>(value) - minValue;
    const int64_t level = offset * static_cast<int64_t>(levels) / valueCount;
    return static_cast<uint8_t>(
        level < static_cast<int64_t>(levels) ? level : levels - 1);
}
} // namespace

bool resolvePetStateStatusLevel(
    const StatusSetCondition &condition,
    const ActivePetState &activePetState,
    int64_t &level)
{
    const uint16_t selectedPetStateMask = condition.petStateMask;
    if (condition.levels == 0 ||
        (!activePetState.isDefault && activePetState.slot >= 16))
        return false;

    const bool selected = !activePetState.isDefault &&
        (selectedPetStateMask & static_cast<uint16_t>(1U << activePetState.slot)) != 0;
    if (!selected)
    {
        level = condition.levels - 1U;
        return true;
    }
    uint8_t rank = 0;
    for (uint8_t slot = 0; slot < activePetState.slot; ++slot)
    {
        if ((selectedPetStateMask & static_cast<uint16_t>(1U << slot)) == 0)
            continue;
        ++rank;
    }
    level = rank;
    return true;
}

bool resolveStatusSet(
    const StatusSetConfig &set,
    StatusValueSource valueSource,
    const void *valueContext,
    StatusSetResolution &resolution)
{
    resolution = {};
    if (!set.animation.valid())
        return false;
    resolution.animation = set.animation;
    if (set.conditionCount == 0)
    {
        resolution.playOnce = true;
        resolution.requiredVersions = 1;
        return true;
    }
    if (set.conditionCount > kMaxStatusConditions || valueSource == nullptr)
        return false;

    uint16_t version = 0;
    for (uint8_t index = 0; index < set.conditionCount; ++index)
    {
        const StatusSetCondition &condition = set.conditions[index];
        int64_t value = 0;
        if (!valueSource(condition, valueContext, value))
            return false;
        if (condition.levels == 0)
            return false;
        version = static_cast<uint16_t>(
            version * condition.levels +
            levelForValue(value, condition.minValue, condition.maxValue, condition.levels));
    }

    if (version >= AssetData::kMaxVersions)
        return false;
    resolution.versionIndex = static_cast<uint8_t>(version);
    resolution.requiredVersions = set.versionCount;
    resolution.playOnce = true;
    return true;
}
