#include <assert.h>
#include "commands/domain/StatusSetContract.h"

namespace
{
bool readValue(const StatusSetCondition &condition, const void *, int32_t &value)
{
    if (condition.source != StatusConditionSource::PetStat || condition.statSlot != 0)
        return false;
    value = 100;
    return true;
}

void testRuntimeContractStatusResolution()
{
    StatusSetConfig set = {};
    set.animation.animationId = 1;
    set.conditions[0].source = StatusConditionSource::PetStat;
    set.conditions[0].statSlot = 0;
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
} // namespace

int main()
{
    testRuntimeContractStatusResolution();
    return 0;
}
