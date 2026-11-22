#pragma once

#include <stdint.h>

struct StatEventSettings
{
    uint32_t statEventTriggersOffset;
    uint32_t statEventNodesOffset;
    uint32_t statEventEdgesOffset;
    uint32_t statEventConditionsOffset;
    uint32_t statEventEffectsOffset;
    uint32_t statEventPagesOffset;
    uint16_t statEventTriggerCount;
    uint16_t statEventNodeCount;
    uint16_t statEventPageCount;
    bool persistentStatEvents;
};
