#pragma once

#include "common/AppProfile.h"

#include <stdint.h>

struct CollectionRoundStats
{
    uint32_t catches;
    uint32_t groupCatches[64];
    uint8_t distinctInGroup[64];
    uint32_t statIncreases[13];
};

#if ENABLE_COLLECTION_GAME_EVENTS
#include <SdFat.h>

class Pet;
struct RuntimeTableConfig;
struct PersistedPetState;

enum class CollectionGameEvent : uint8_t { StatTransition, CaptureCatch, CaptureRound };

// Use the caller's existing transaction snapshot to roll back on an SD failure.
bool recordCollectionGameEvent(SdFat *sd, const RuntimeTableConfig &config, Pet &pet,
                               const PersistedPetState &before, CollectionGameEvent event,
                               uint8_t shopItem = 255, const CollectionRoundStats *round = nullptr);
#endif
