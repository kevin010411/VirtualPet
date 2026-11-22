#pragma once

#include "common/AppProfile.h"
#include <stdint.h>

struct CollectionSettings
{
#if ENABLE_COLLECTION_GAME_EVENTS
    uint64_t captureOnlyMask;
#endif
    uint32_t collectionFingerprint;
    uint32_t trackedActionFingerprint;
#if ENABLE_COLLECTION_GAME_EVENTS
    uint32_t collectionProgressFingerprint;
    bool retainCollectionOnRestart;
#endif
};
