#include <assert.h>
#include <limits.h>

#include "pet_behavior/domain/PetBehaviorStatSlot.h"
#include "pet_behavior/domain/RuntimeValueResolver.h"

namespace
{
RuntimeValueContext petContext(PetBehaviorConfig &config,
                               const int16_t *values,
                               uint8_t capacity)
{
    RuntimeValueContext context = {};
    context.petStats = values;
    context.petStatCapacity = capacity;
    context.behaviorConfig = &config;
    context.stageDays = 0;
    return context;
}

void testStageBoundaries()
{
    PetBehaviorConfig config = {};
    int16_t values[1] = {};
    RuntimeValueContext context = petContext(config, values, 1);
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
    RuntimeValueContext context = petContext(config, values, 1);
    int32_t value = 0;

    assert(!resolveRuntimeValue(99, context, value));
    assert(!matchesRuntimeRange(
        {runtimeValueIdForPetStat(0), 10, -10}, context));
    config.stats[0].active = false;
    assert(!resolveRuntimeValue(runtimeValueIdForPetStat(0), context, value));
    config.stats[0].active = true;
    context.petStatCapacity = 0;
    assert(!resolveRuntimeValue(runtimeValueIdForPetStat(0), context, value));
    context.petStatCapacity = 1;
    context.petStats = nullptr;
    assert(!resolveRuntimeValue(runtimeValueIdForPetStat(0), context, value));
}

void testCompiledDomain()
{
    PetBehaviorConfig config = {};
    config.statCount = 1;
    config.stats[0].active = true;
    config.stats[0].minValue = -10;
    config.stats[0].maxValue = 10;
    assert(runtimeRangeWithinCompiledDomain(
        {runtimeValueIdForPetStat(0), -10, 10}, config));
    assert(!runtimeRangeWithinCompiledDomain(
        {runtimeValueIdForPetStat(0), -11, 10}, config));
    assert(!runtimeRangeWithinCompiledDomain(
        {kRuntimeValueStageDays, 0, 3651}, config));
}
} // namespace

int main()
{
    testStageBoundaries();
    testInvalidReferencesFailClosed();
    testCompiledDomain();
    return 0;
}
