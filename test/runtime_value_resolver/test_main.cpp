#include <assert.h>
#include <limits.h>

#include "pet_behavior/domain/PetBehaviorStatSlot.h"
#include "pet_behavior/domain/RuntimeValueResolver.h"

namespace
{
RuntimeValueContext petContext(const int16_t *values, uint16_t activeMask)
{
    RuntimeValueContext context = {};
    context.petStats = values;
    context.activePetStatMask = activeMask;
    context.stageDays = 0;
    return context;
}

void testStageBoundaries()
{
    int16_t values[1] = {};
    RuntimeValueContext context = petContext(values, 1U);
    int32_t value = -1;

    context.stageDays = 0;
    assert(resolveRuntimeValue(kRuntimeValueStageDays, context, value));
    assert(value == 0);
    context.stageDays = 3650;
    assert(resolveRuntimeValue(kRuntimeValueStageDays, context, value));
    assert(value == 3650);
    assert(matchesRuntimeRange(
        {kRuntimeValueStageDays, 0, 3650}, context));
    assert(matchesRuntimeRange(
        {kRuntimeValueStageDays, 3650, 3650}, context));
    assert(!matchesRuntimeRange(
        {kRuntimeValueStageDays, 3651, 3650}, context));
    context.stageDays = static_cast<uint32_t>(INT32_MAX) + 1U;
    assert(!resolveRuntimeValue(kRuntimeValueStageDays, context, value));
}

void testInvalidReferencesFailClosed()
{
    PetBehaviorConfig config = {};
    config.statCount = 1;
    config.stats[0].active = true;
    config.stats[0].minValue = -10;
    config.stats[0].maxValue = 10;
    int16_t values[1] = {5};
    RuntimeValueContext context = petContext(values, activePetBehaviorStatMask(config));
    int32_t value = 0;

    assert(!resolveRuntimeValue(99, context, value));
    assert(!matchesRuntimeRange(
        {runtimeValueIdForPetStat(0), 10, -10}, context));
    config.stats[0].active = false;
    context.activePetStatMask = activePetBehaviorStatMask(config);
    assert(!resolveRuntimeValue(runtimeValueIdForPetStat(0), context, value));
    context.petStats = nullptr;
    assert(!resolveRuntimeValue(runtimeValueIdForPetStat(0), context, value));
}

void testExecutableShape()
{
    assert(isRuntimeBehaviorRange({runtimeValueIdForPetStat(0), -10, 10}));
    assert(isRuntimeBehaviorRange({runtimeValueIdForPetStat(0), -11, 10}));
    assert(isRuntimeBehaviorRange({kRuntimeValueStageDays, 0, 3651}));
    assert(!isRuntimeBehaviorRange({kRuntimeValueSpeciesSlot, 1, 2}));
    assert(isRuntimeRangeWellFormed({kRuntimeValueSpeciesSlot, 1, 2}));
    assert(!isRuntimeRangeWellFormed({kRuntimeValueStageDays, 2, 1}));
}
} // namespace

int main()
{
    testStageBoundaries();
    testInvalidReferencesFailClosed();
    testExecutableShape();
    return 0;
}
