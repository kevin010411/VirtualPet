#include "pet/PetStatEvents.h"

#if ENABLE_STAT_EVENTS
#include "common/SdBinaryRead.h"
#include "pet/PetBehaviorRuntimeRules.h"
#include "pet/PetBehaviorStatSlot.h"
#include "resources/detail/RuntimeTableReader.h"

namespace
{
constexpr uint8_t kEventTerminal = 1;
constexpr uint8_t kEventWait = 2;
using RuntimeTableInternal::readI64;
using RuntimeTableInternal::readU16;
using RuntimeTableInternal::readU32;

class EventFile
{
public:
    ~EventFile() { file.close(); }

    bool open(SdFat *sd) { return sd != nullptr && file.open("/runtime.bin", FILE_READ); }

    template <size_t Size>
    bool read(uint32_t offset, uint16_t index, uint8_t (&record)[Size])
    {
        return file.seekSet(offset + static_cast<uint32_t>(index) * static_cast<uint32_t>(Size)) &&
               readSdBinary(file, record, Size) == static_cast<int>(Size);
    }

private:
    SdBaseFile file;
};

bool compare(uint8_t comparator, int64_t lhs, int64_t rhs)
{
    switch (comparator)
    {
    case 0: return lhs < rhs;
    case 1: return lhs <= rhs;
    case 2: return lhs == rhs;
    case 3: return lhs >= rhs;
    case 4: return lhs > rhs;
    default: return false;
    }
}

bool matchesTrigger(uint8_t mode, uint32_t threshold, uint32_t before, uint32_t after)
{
    if (mode == 0) return before != after;
    const uint8_t comparator = mode - 1;
    return !compare(comparator, before, threshold) && compare(comparator, after, threshold);
}

bool resolveEventValue(RuntimeValueId id, const RuntimeValueContext &context, int64_t &value)
{
    if (!resolveRuntimeValue(id, context, value)) return false;
    if (id == kRuntimeValueOutfitSlot) value += static_cast<uint16_t>(context.speciesSlot) * 256U;
    return true;
}

bool matchesConditions(EventFile &file, const StatEventSettings &events,
                       uint16_t first, uint8_t count, const RuntimeValueContext &context,
                       bool &matches)
{
    matches = true;
    for (uint8_t offset = 0; offset < count; ++offset)
    {
        uint8_t condition[8];
        if (!file.read(events.statEventConditionsOffset, first + offset, condition)) return false;
        int64_t lhs = 0;
        int64_t rhs = readU32(condition + 4);
        if (!resolveEventValue(condition[0], context, lhs) ||
            (condition[2] && !resolveEventValue(static_cast<RuntimeValueId>(rhs), context, rhs)) ||
            !compare(condition[1], lhs, rhs))
        {
            matches = false;
            break;
        }
    }
    return true;
}

bool applyNode(EventFile &file, const StatEventSettings &events, const PetBehaviorConfig &config, const uint8_t (&node)[12],
               PetStatEventResult &result)
{
    const uint16_t firstEffect = readU16(node + 2);
    for (uint8_t offset = 0; offset < node[9]; ++offset)
    {
        uint8_t effect[12];
        if (!file.read(events.statEventEffectsOffset, firstEffect + offset, effect)) return false;
        const uint8_t slot = effect[0];
        if (slot == kStageDaysEffectSlot)
        {
            result.values.stage_days = applyStageDaysEffect(result.values.stage_days,
                static_cast<PetBehaviorEffectOperation>(effect[1]), readI64(effect + 4));
            continue;
        }
        if (slot >= config.statCount) return false;
        uint32_t &value = result.values.customStats[slot];
        value = applyPetBehaviorStatEffect(config.stats[slot], value,
                                          static_cast<PetBehaviorEffectOperation>(effect[1]),
                                          readI64(effect + 4));
    }
    if (node[10] > sizeof(result.pages) / sizeof(result.pages[0]) - result.pageCount) return false;
    const uint16_t firstPage = readU16(node + 4);
    for (uint8_t offset = 0; offset < node[10]; ++offset)
        result.pages[result.pageCount++] = firstPage + offset;
    result.terminal = (node[11] & kEventTerminal) != 0;
    return true;
}

bool runEvent(EventFile &file, const StatEventSettings &events, const PetBehaviorConfig &config, uint16_t nodeIndex,
              const PetStatSnapshot &before, const PetStatSnapshot &after,
              PetStatEventResult &result, bool resume = false)
{
    const uint16_t activeMask = activePetBehaviorStatMask(config);
    // Immediate paths are acyclic. Waiting nodes may be revisited on a later mutation.
    for (uint16_t remaining = events.statEventNodeCount; remaining != 0; --remaining)
    {
        uint8_t node[12];
        if (nodeIndex >= events.statEventNodeCount ||
            !file.read(events.statEventNodesOffset, nodeIndex, node)) return false;
        if (!resume)
        {
            if (!applyNode(file, events, config, node, result)) return false;
            if (result.terminal)
            {
                result.values.activeStatEvent = kNoActiveStatEvent;
                return true;
            }
            if (events.persistentStatEvents)
            {
                result.values.activeStatEvent = nodeIndex;
                if ((node[11] & kEventWait) || !node[8]) return true;
            }
        }
        resume = false;

        const uint16_t firstEdge = readU16(node);
        bool hasNext = false;
        for (uint8_t offset = 0; offset < node[8]; ++offset)
        {
            uint8_t edge[8];
            if (!file.read(events.statEventEdgesOffset, firstEdge + offset, edge)) return false;
            const uint16_t changeSlot = readU16(edge + 6);
            if (changeSlot && before.customStats[changeSlot - 1U] == after.customStats[changeSlot - 1U]) continue;
            const PetStatSnapshot &snapshot = edge[5] == 1 ? before : edge[5] == 2 ? after : result.values;
            const RuntimeValueContext context = makeRuntimeValueContext(
                snapshot.customStats, activeMask, snapshot.stage_days, snapshot.speciesSlot, snapshot.outfitSlot);
            if (!matchesConditions(file, events, readU16(edge + 2), edge[4], context, hasNext)) return false;
            if (hasNext)
            {
                nodeIndex = readU16(edge);
                break;
            }
        }
        if (!hasNext) return true;
    }
    return false;
}
} // namespace

bool settleStatEvents(SdFat *sd, const StatEventSettings &events, const PetBehaviorConfig &config,
                      const PetStatSnapshot &before, const PetStatSnapshot &after,
                      PetStatEventResult &result)
{
    result.values = after;
    result.values.activeStatEvent = before.activeStatEvent;
    result.terminal = false;
    result.pageCount = 0;
    if (events.statEventTriggerCount == 0) return true;
    EventFile file;
    if (!file.open(sd)) return false;
    if (events.persistentStatEvents && before.activeStatEvent != kNoActiveStatEvent)
    {
        bool changed = before.stage_days != after.stage_days;
        for (uint8_t slot = 0; slot < config.statCount && !changed; ++slot)
            changed = before.customStats[slot] != after.customStats[slot];
        if (!changed) return true;
        return runEvent(file, events, config, before.activeStatEvent, before, after, result, true);
    }
    for (uint16_t index = 0; index < events.statEventTriggerCount && !result.terminal; ++index)
    {
        uint8_t trigger[8];
        if (!file.read(events.statEventTriggersOffset, index, trigger)) return false;
        const uint8_t slot = trigger[2];
        if (slot >= config.statCount) return false;
        if (!matchesTrigger(trigger[3], readU32(trigger + 4), before.customStats[slot], after.customStats[slot]))
            continue;
        if (!runEvent(file, events, config, readU16(trigger), before, after, result)) return false;
        if (events.persistentStatEvents) break;
    }
    return true;
}

bool readStatEventPage(SdFat *sd, const StatEventSettings &events, uint16_t index, StatEventPage &result)
{
    EventFile file;
    uint8_t page[4];
    if (index >= events.statEventPageCount || !file.open(sd) ||
        !file.read(events.statEventPagesOffset, index, page)) return false;
    result = {readU16(page), readU16(page + 2)};
    return true;
}
#endif
