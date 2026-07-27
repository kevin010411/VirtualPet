#include <assert.h>
#include <stdint.h>
#include <string.h>

#include "commands/domain/StatusSetContract.h"

namespace
{
struct Values
{
    int32_t hunger = 0;
    int32_t mood = 0;
    int32_t sick = 0;
    int32_t stageDays = 0;
    int32_t custom0 = 0;
    int32_t custom7 = 0;
};

bool valueForSource(const char *source, const void *context, int32_t &value)
{
    const Values &values = *static_cast<const Values *>(context);
    if (strcmp(source, "hunger") == 0)
        value = values.hunger;
    else if (strcmp(source, "mood") == 0)
        value = values.mood;
    else if (strcmp(source, "sick") == 0)
        value = values.sick;
    else if (strcmp(source, "stage_days") == 0)
        value = values.stageDays;
    else if (strcmp(source, "custom0") == 0)
        value = values.custom0;
    else if (strcmp(source, "custom7") == 0)
        value = values.custom7;
    else
        return false;
    return true;
}

StatusSetsConfig parse(const char *contract)
{
    StatusSetsConfig config = {};
    assert(parseStatusSetsContract(contract, config));
    return config;
}

StatusSetResolution resolve(const StatusSetConfig &set, const Values &values)
{
    StatusSetResolution resolution = {};
    assert(resolveStatusSet(set, valueForSource, &values, resolution));
    return resolution;
}

void assertRejected(const char *contract)
{
    StatusSetsConfig config = {};
    assert(!parseStatusSetsContract(contract, config));
}

void testUnconditionalContract()
{
    const StatusSetsConfig config = parse("version=1\nStatus|\n");
    assert(config.count == 1);
    const StatusSetResolution resolution = resolve(config.sets[0], Values{});
    assert(strcmp(resolution.animation, "Status") == 0);
    assert(resolution.playOnce);
    assert(resolution.frame == 0);
    assert(resolution.requiredFrames == 0);
}

void testSingleConditionMinimumAndMaximum()
{
    const StatusSetsConfig config =
        parse("version=1\nStatusHungry|hunger:5:0:100\n");
    Values values = {};
    values.hunger = 0;
    assert(resolve(config.sets[0], values).frame == 1);
    values.hunger = 100;
    const StatusSetResolution maximum = resolve(config.sets[0], values);
    assert(maximum.frame == 5);
    assert(maximum.requiredFrames == 5);
    assert(!maximum.playOnce);
}

void testCanonicalMixedRadixAndSickness()
{
    const StatusSetsConfig config = parse(
        "version=1\n"
        "StatusHungryMoodSick|hunger:3:0:100,mood:4:0:100,sick:2:0:1\n");
    Values values = {};
    assert(resolve(config.sets[0], values).frame == 1);
    values.hunger = 100;
    values.sick = 1;
    assert(resolve(config.sets[0], values).frame == 18);
    values.mood = 100;
    assert(resolve(config.sets[0], values).frame == 24);
}

void testCustomConditionsAndLongestSupportedName()
{
    const StatusSetsConfig config = parse(
        "version=1\n"
        "StatusStageDaysCustom0Custom7|"
        "stage_days:2:0:3650,custom0:2:-50:50,custom7:2:0:100\n");
    assert(strlen(config.sets[0].animation) < kStatusAnimationNameSize);
    Values values = {};
    values.stageDays = 3650;
    values.custom0 = 50;
    values.custom7 = 100;
    const StatusSetResolution resolution = resolve(config.sets[0], values);
    assert(strcmp(resolution.animation, "StatusStageDaysCustom0Custom7") == 0);
    assert(resolution.frame == 8);
}

void testMultipleSetsAndFrameBudgetBoundary()
{
    const StatusSetsConfig multiple = parse(
        "version=1\n"
        "StatusAge|age:1:0:3650\n"
        "StatusHungry|hunger:1:0:100\n"
        "StatusMood|mood:1:0:100\n");
    assert(multiple.count == 3);

    const StatusSetsConfig boundary = parse(
        "version=1\n"
        "StatusHungryMood|hunger:32:0:100,mood:8:0:100\n");
    Values values = {};
    values.hunger = 100;
    values.mood = 100;
    const StatusSetResolution resolution = resolve(boundary.sets[0], values);
    assert(resolution.requiredFrames == 256);
    assert(resolution.frame == 256);
}

void testMalformedAndUnsupportedContractsFailSafely()
{
    assertRejected("");
    assertRejected("version=2\nStatus|\n");
    assertRejected("version=1\n");
    assertRejected("version=1\nStatus|hunger:1:0:100\n");
    assertRejected("version=1\nStatusHungry|hunger,5,0,100\n");
    assertRejected("version=1\nStatusHungry|satiety:5:0:100\n");
    assertRejected("version=1\nStatusMoodHungry|mood:2:0:100,hunger:2:0:100\n");
    assertRejected("version=1\nStatusHungry|mood:2:0:100\n");
    assertRejected("version=1\nStatusSick|sick:3:0:1\n");
    assertRejected(
        "version=1\n"
        "StatusHungryMood|hunger:32:0:100,mood:32:0:100\n");
    assertRejected(
        "version=1\n"
        "StatusHungry|hunger:2:0:100\n"
        "StatusHungry|hunger:4:0:100\n");
}

void testMissingValueFailsSafely()
{
    const StatusSetsConfig config =
        parse("version=1\nStatusAge|age:2:0:3650\n");
    StatusSetResolution resolution = {};
    const Values values = {};
    assert(!resolveStatusSet(config.sets[0], valueForSource, &values, resolution));
}
} // namespace

int main()
{
    testUnconditionalContract();
    testSingleConditionMinimumAndMaximum();
    testCanonicalMixedRadixAndSickness();
    testCustomConditionsAndLongestSupportedName();
    testMultipleSetsAndFrameBudgetBoundary();
    testMalformedAndUnsupportedContractsFailSafely();
    testMissingValueFailsSafely();
    return 0;
}
