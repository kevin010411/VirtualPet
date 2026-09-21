#include <assert.h>
#include "commands/domain/StatusSetContract.h"

namespace
{
bool readValue(const StatusSetCondition &condition, const void *, int32_t &value)
{
    if (condition.source != StatusConditionSource::PetStat ||
        condition.valueId != runtimeValueIdForPetStat(0))
        return false;
    value = 100;
    return true;
}

void testRuntimeContractStatusResolution()
{
    StatusSetConfig set = {};
    set.animation.animationId = 1;
    set.conditions[0].source = StatusConditionSource::PetStat;
    set.conditions[0].valueId = runtimeValueIdForPetStat(0);
    set.conditions[0].levels = 2;
    set.conditions[0].minValue = 0;
    set.conditions[0].maxValue = 100;
    set.conditionCount = 1;
    set.versionCount = 2;

    StatusSetResolution resolution = {};
    assert(resolveStatusSet(set, readValue, nullptr, resolution));
    assert(resolution.versionIndex == 1);
    assert(resolution.requiredVersions == 2);
    assert(resolution.playOnce);
}

void testPetStateMaskShape()
{
    StatusSetCondition condition = {};
    condition.source = StatusConditionSource::PetStatus;
    condition.petStateMask = 0x0005;
    condition.levels = 3;
    assert(condition.petStateMask == 0x0005);
    assert(condition.valueId == 0);
}
} // namespace

int main()
{
    testRuntimeContractStatusResolution();
    testPetStateMaskShape();
    return 0;
}
