#ifndef PET_BEHAVIOR_STAT_SLOT_H
#define PET_BEHAVIOR_STAT_SLOT_H

#include <stdint.h>

#include "pet/PetBehaviorTypes.h"

static_assert(kPetBehaviorSlotCount <= 12, "Pet Stat capacity must fit the runtime contract.");

inline uint16_t activePetBehaviorStatMask(const PetBehaviorConfig &config)
{
    uint16_t mask = 0;
    for (uint8_t slot = 0; slot < kPetBehaviorSlotCount; ++slot)
        if (config.stats[slot].active)
            mask |= static_cast<uint16_t>(1U << slot);
    return mask;
}

inline bool parsePetBehaviorStatSlot(const char *token, uint8_t &slot)
{
    if (token == nullptr ||
        token[0] != 'c' || token[1] != 'u' || token[2] != 's' ||
        token[3] != 't' || token[4] != 'o' || token[5] != 'm' ||
        token[6] < '0' || token[6] > '9')
    {
        return false;
    }

    uint8_t parsed = static_cast<uint8_t>(token[6] - '0');
    if (token[7] != '\0')
    {
        if (token[6] != '1' || (token[7] != '0' && token[7] != '1') || token[8] != '\0')
            return false;
        parsed = static_cast<uint8_t>(10 + token[7] - '0');
    }
    if (parsed >= kPetBehaviorSlotCount)
        return false;
    slot = parsed;
    return true;
}

class ActivePetBehaviorStatSlots
{
public:
    ActivePetBehaviorStatSlots() = default;

    explicit ActivePetBehaviorStatSlots(const PetBehaviorConfig &config)
    {
        configure(config);
    }

    void configure(const PetBehaviorConfig &config)
    {
        activeMask = activePetBehaviorStatMask(config);
    }

    bool resolve(const char *token, uint8_t &slot) const
    {
        return parsePetBehaviorStatSlot(token, slot) && contains(slot);
    }

    bool contains(uint8_t slot) const
    {
        return slot < kPetBehaviorSlotCount &&
               (activeMask & static_cast<uint16_t>(1U << slot)) != 0;
    }

    uint16_t mask() const { return activeMask; }

private:
    uint16_t activeMask = 0;
};

#endif // PET_BEHAVIOR_STAT_SLOT_H
