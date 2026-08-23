#ifndef PET_BEHAVIOR_STAT_SLOT_H
#define PET_BEHAVIOR_STAT_SLOT_H

#include <stdint.h>

#include "pet_behavior/domain/PetBehaviorTypes.h"

static_assert(kPetBehaviorSlotCount <= 10, "Pet Stat Slot tokens require one decimal digit.");

inline bool parsePetBehaviorStatSlot(const char *token, uint8_t &slot)
{
    if (token == nullptr ||
        token[0] != 'c' || token[1] != 'u' || token[2] != 's' ||
        token[3] != 't' || token[4] != 'o' || token[5] != 'm' ||
        token[6] < '0' || token[6] >= '0' + kPetBehaviorSlotCount ||
        token[7] != '\0')
    {
        return false;
    }

    slot = static_cast<uint8_t>(token[6] - '0');
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
        for (uint8_t slot = 0; slot < kPetBehaviorSlotCount; ++slot)
            active[slot] = config.stats[slot].active;
    }

    bool resolve(const char *token, uint8_t &slot) const
    {
        return parsePetBehaviorStatSlot(token, slot) && active[slot];
    }

private:
    bool active[kPetBehaviorSlotCount] = {};
};

#endif // PET_BEHAVIOR_STAT_SLOT_H
