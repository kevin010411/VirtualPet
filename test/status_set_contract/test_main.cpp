#include <assert.h>
#include "controller/StatusSetContract.h"
#include "pet/PetStateClassifier.h"

namespace
{
bool readValue(const StatusSetCondition &condition, const void *, int32_t &value)
{
    if (condition.kind != StatusConditionKind::RuntimeValue ||
        condition.valueId != runtimeValueIdForPetStat(0))
        return false;
    value = 100;
    return true;
}

void testRuntimeContractStatusResolution()
{
    StatusSetConfig set = {};
    set.animation.animationId = 1;
    set.conditions[0].kind = StatusConditionKind::RuntimeValue;
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

void testPetStateSubsetRanksAndOtherFallback()
{
    int32_t level = -1;
    StatusSetCondition condition = {};
    condition.petStateMask = 0x8201U; // slots 0, 9, and 15
    condition.levels = 4;
    ActivePetState active = {};
    active.isDefault = false;
    active.slot = 0;
    assert(resolvePetStateStatusLevel(condition, active, level));
    assert(level == 0);
    active.slot = 9;
    assert(resolvePetStateStatusLevel(condition, active, level));
    assert(level == 1);
    active.slot = 15;
    assert(resolvePetStateStatusLevel(condition, active, level));
    assert(level == 2);

    // Unselected named states and Default intentionally share Other.
    active.slot = 8;
    assert(resolvePetStateStatusLevel(condition, active, level));
    assert(level == 3);
    active = {};
    assert(resolvePetStateStatusLevel(condition, active, level));
    assert(level == 3);
}

void testLowerSelectedSlotCountDeterminesRank()
{
    int32_t level = -1;
    StatusSetCondition condition = {};
    condition.levels = 4;
    ActivePetState active = {};
    active.isDefault = false;
    active.slot = 8;
    condition.petStateMask = 0x0103U;
    assert(resolvePetStateStatusLevel(condition, active, level));
    assert(level == 2);
    condition.petStateMask = 0x0301U;
    assert(resolvePetStateStatusLevel(condition, active, level));
    assert(level == 1);
}

void testPetStateLevelShapeFailsClosed()
{
    int32_t level = -1;
    StatusSetCondition condition = {};
    condition.levels = 1;
    ActivePetState active = {};
    active.isDefault = false;
    active.slot = 0;
    assert(!resolvePetStateStatusLevel(condition, active, level));
    condition.petStateMask = 0x0005U;
    condition.levels = 2;
    assert(!resolvePetStateStatusLevel(condition, active, level));
    condition.levels = 3;
    active.slot = 16;
    assert(!resolvePetStateStatusLevel(condition, active, level));
}
} // namespace

int main()
{
    testRuntimeContractStatusResolution();
    testPetStateSubsetRanksAndOtherFallback();
    testLowerSelectedSlotCountDeterminesRank();
    testPetStateLevelShapeFailsClosed();
    return 0;
}
