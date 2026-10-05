#include <assert.h>

#include "pet/PetStateClassifier.h"

namespace
{
AssetData::AnimationRef animation(uint16_t id)
{
    AssetData::AnimationRef result = {};
    result.speciesSlot = 1;
    result.outfitSlot = 1;
    result.animationId = id;
    return result;
}

PetBehaviorConfig configWithStates()
{
    PetBehaviorConfig config = {};
    config.statCount = 1;
    config.stats[0].active = true;
    config.stats[0].minValue = 0;
    config.stats[0].maxValue = 100;
    config.idleAnimation = animation(99);
    return config;
}

RuntimePetStateConfig state(int32_t minimum, int32_t maximum, uint16_t id)
{
    RuntimePetStateConfig result = {};
    result.predicate = {runtimeValueIdForPetStat(0), minimum, maximum};
    result.idleAnimation = animation(id);
    return result;
}

void testFirstMatchWins()
{
    PetBehaviorConfig config = configWithStates();
    config.petStates[0] = state(0, 60, 1);
    config.petStates[1] = state(40, 100, 2);
    config.petStateCount = 2;
    PetStatSnapshot snapshot = {};
    snapshot.customStats[0] = 50;

    const ActivePetState active = PetStateClassifier::classify(config, snapshot);
    assert(!active.isDefault);
    assert(active.slot == 0);
    assert(active.idleAnimation.animationId == 1);
}

void testDefaultWhenNoStateMatches()
{
    PetBehaviorConfig config = configWithStates();
    config.petStates[0] = state(0, 10, 1);
    config.petStateCount = 1;
    PetStatSnapshot snapshot = {};
    snapshot.customStats[0] = 50;

    const ActivePetState active = PetStateClassifier::classify(config, snapshot);
    assert(active.isDefault);
    assert(active.slot == kDefaultPetStateSlot);
    assert(active.idleAnimation.animationId == 99);
}

void testFirstAndSixteenthStatesAndFreshSnapshot()
{
    PetBehaviorConfig config = configWithStates();
    for (uint8_t slot = 0; slot < kMaxRuntimePetStates; ++slot)
        config.petStates[slot] = state(slot, slot, static_cast<uint16_t>(slot + 1));
    config.petStateCount = kMaxRuntimePetStates;
    PetStatSnapshot snapshot = {};

    snapshot.customStats[0] = 0;
    ActivePetState active = PetStateClassifier::classify(config, snapshot);
    assert(active.slot == 0);
    assert(active.idleAnimation.animationId == 1);

    snapshot.customStats[0] = 15;
    active = PetStateClassifier::classify(config, snapshot);
    assert(active.slot == 15);
    assert(active.idleAnimation.animationId == 16);
}
} // namespace

int main()
{
    testFirstMatchWins();
    testDefaultWhenNoStateMatches();
    testFirstAndSixteenthStatesAndFreshSnapshot();
    return 0;
}
