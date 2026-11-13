#include <assert.h>
#include <vector>

#include "pet/PetStatEvents.h"

namespace
{
void append(std::vector<uint8_t> &bytes, std::initializer_list<uint8_t> record)
{
    bytes.insert(bytes.end(), record);
}

PetBehaviorConfig fixture(std::vector<uint8_t> &bytes)
{
    PetBehaviorConfig config = {};
    config.statCount = kPetBehaviorSlotCount;
    for (auto &stat : config.stats)
    {
        stat.active = true;
        stat.maxValue = 300;
    }

    config.statEventTriggersOffset = bytes.size();
    config.statEventTriggerCount = 3;
    append(bytes, {0, 0, 11, 3, 3, 0, 0, 0}); // Service day becomes 3.
    append(bytes, {3, 0, 11, 4, 3, 0, 0, 0}); // Service day crosses >= 3.
    append(bytes, {4, 0, 10, 0, 0, 0, 0, 0}); // Stat 10 changes in the base mutation.

    config.statEventNodesOffset = bytes.size();
    config.statEventNodeCount = 5;
    append(bytes, {0, 0, 0, 0, 0, 0, 0, 0, 2, 1, 0, 0}); // Subtract and branch.
    append(bytes, {2, 0, 0, 0, 0, 0, 0, 0, 2, 0, 1, 0}); // Check before and after.
    append(bytes, {0, 0, 1, 0, 1, 0, 0, 0, 0, 1, 1, 1}); // Terminal with page.
    append(bytes, {0, 0, 2, 0, 2, 0, 0, 0, 0, 1, 1, 0}); // Fallback and second root.
    append(bytes, {0, 0, 3, 0, 3, 0, 0, 0, 0, 1, 0, 0}); // Would alter Stat 9.

    config.statEventEdgesOffset = bytes.size();
    append(bytes, {1, 0, 0, 0, 2, 0, 0, 0}); // Current: stat < 150 and outfit is 2/3.
    append(bytes, {3, 0, 0, 0, 0, 0, 0, 0});
    append(bytes, {2, 0, 2, 0, 2, 1, 0, 0}); // Before: stage day and Stat 10 == Stat 8.
    append(bytes, {2, 0, 4, 0, 1, 2, 0, 0}); // After: stat still >= 150.

    config.statEventConditionsOffset = bytes.size();
    append(bytes, {26, 0, 0, 0, 150, 0, 0, 0});
    append(bytes, {2, 2, 0, 0, 3, 2, 0, 0});
    append(bytes, {0, 2, 0, 0, 2, 0, 0, 0});
    append(bytes, {26, 2, 1, 0, 24, 0, 0, 0});
    append(bytes, {26, 3, 0, 0, 150, 0, 0, 0});

    config.statEventEffectsOffset = bytes.size();
    append(bytes, {10, 0, 0, 0, 226, 255, 255, 255, 255, 255, 255, 255});
    append(bytes, {9, 1, 0, 0, 4, 0, 0, 0, 0, 0, 0, 0});
    append(bytes, {8, 1, 0, 0, 144, 1, 0, 0, 0, 0, 0, 0});
    append(bytes, {9, 1, 0, 0, 7, 0, 0, 0, 0, 0, 0, 0});

    config.statEventPagesOffset = bytes.size();
    config.statEventPageCount = 3;
    append(bytes, {45, 0, 160, 0});
    append(bytes, {46, 0, 0, 1});
    append(bytes, {47, 0, 5, 0});
    return config;
}

void testPersistentWaitingEvent()
{
    std::vector<uint8_t> bytes;
    PetBehaviorConfig config = {};
    config.persistentStatEvents = true;
    config.statCount = 3;
    for (auto &stat : config.stats)
    {
        stat.active = true;
        stat.maxValue = 100;
    }
    config.statEventTriggerCount = 1;
    append(bytes, {0, 0, 0, 0, 0, 0, 0, 0});
    config.statEventNodesOffset = bytes.size();
    config.statEventNodeCount = 3;
    append(bytes, {0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0});
    append(bytes, {1, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 2});
    append(bytes, {2, 0, 1, 0, 1, 0, 0, 0, 0, 1, 0, 1});
    config.statEventEdgesOffset = bytes.size();
    append(bytes, {1, 0, 0, 0, 0, 0, 0, 0});
    append(bytes, {2, 0, 0, 0, 1, 0, 1, 0});
    config.statEventConditionsOffset = bytes.size();
    append(bytes, {17, 3, 0, 0, 3, 0, 0, 0});
    config.statEventEffectsOffset = bytes.size();
    append(bytes, {2, 0, 0, 0, 10, 0, 0, 0, 0, 0, 0, 0});
    append(bytes, {2, 0, 0, 0, 5, 0, 0, 0, 0, 0, 0, 0});
    SdFat sd;
    sd.seedReadOnly("/runtime.bin", bytes);
    PetStatSnapshot before = {}, after = {};
    after.customStats[0] = 1;
    PetStatEventResult result = {};
    assert(settleStatEvents(&sd, config, before, after, result));
    assert(result.values.activeStatEvent == 1 && result.values.customStats[2] == 10 && result.pageCount == 1);
    before = after = result.values;
    after.customStats[0] = 2;
    after.customStats[1] = 2;
    assert(settleStatEvents(&sd, config, before, after, result));
    assert(result.values.activeStatEvent == 1 && result.values.customStats[2] == 10 && result.pageCount == 0);
    before = after = result.values;
    after.customStats[1] = 4;
    assert(settleStatEvents(&sd, config, before, after, result));
    assert(result.values.activeStatEvent == 1 && result.values.customStats[2] == 10 && result.pageCount == 0);
    before = after = result.values;
    after.customStats[0] = 3;
    assert(settleStatEvents(&sd, config, before, after, result));
    assert(result.terminal && result.values.activeStatEvent == kNoActiveStatEvent && result.values.customStats[2] == 15);
}
}

int main()
{
    testPersistentWaitingEvent();
    std::vector<uint8_t> bytes;
    PetBehaviorConfig config = fixture(bytes);
    SdFat sd;
    sd.seedReadOnly("/runtime.bin", bytes);

    PetStatSnapshot before = {};
    before.stage_days = 2;
    before.speciesSlot = 2;
    before.outfitSlot = 3;
    before.customStats[8] = 120;
    before.customStats[10] = 120;
    before.customStats[11] = 2;
    PetStatSnapshot after = before;
    after.stage_days = 3;
    after.customStats[10] = 150;
    after.customStats[11] = 3;

    PetStatEventResult result = {};
    assert(settleStatEvents(&sd, config, before, after, result));
    assert(result.terminal && result.pageCount == 2);
    assert(result.pages[0] == 0 && result.pages[1] == 1);
    assert(result.values.customStats[8] == 120 && result.values.customStats[9] == 4);
    assert(result.values.customStats[10] == 120 && result.values.customStats[11] == 3);
    auto daysEffect = bytes;
    daysEffect[config.statEventEffectsOffset + 12] = UINT8_MAX;
    SdFat daysSd;
    daysSd.seedReadOnly("/runtime.bin", daysEffect);
    assert(settleStatEvents(&daysSd, config, before, after, result));
    assert(result.values.stage_days == 4 && result.values.customStats[9] == 0);

    uint16_t animationId = 0;
    uint16_t durationMs = 0;
    assert(readStatEventPage(&sd, config, result.pages[1], animationId, durationMs));
    assert(animationId == 46 && durationMs == 256);
    assert(!readStatEventPage(&sd, config, config.statEventPageCount, animationId, durationMs));

    before.stage_days = 1;
    assert(settleStatEvents(&sd, config, before, after, result));
    assert(result.terminal && result.pageCount == 2); // After branch keeps base value 150.

    after.speciesSlot = 1;
    assert(settleStatEvents(&sd, config, before, after, result));
    assert(!result.terminal && result.pageCount == 2);
    assert(result.pages[0] == 2 && result.pages[1] == 2);
    assert(result.values.customStats[8] == 300 && result.values.customStats[9] == 7);

    assert(settleStatEvents(&sd, config, after, after, result));
    assert(!result.terminal && result.pageCount == 0 && result.values.customStats[10] == 150);
    assert(!settleStatEvents(nullptr, config, before, after, result));

    config.statEventEffectsOffset = bytes.size();
    assert(!settleStatEvents(&sd, config, before, after, result));
    config.statEventTriggerCount = 0;
    assert(settleStatEvents(nullptr, config, before, after, result));
    assert(!result.terminal && result.pageCount == 0 && result.values.customStats[10] == 150);
}
