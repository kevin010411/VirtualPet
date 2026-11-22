#pragma once

#include "resources/AssetRuntimeContract.h"
#if ENABLE_COLLECTION_CATALOG || ENABLE_COLLECTION_GAME_EVENTS
#include "collection/CollectionSettings.h"
#endif

struct RuntimeTableConfig
{
    AssetData::RuntimeManifest assetManifest;
#if ENABLE_COLLECTION_CATALOG || ENABLE_COLLECTION_GAME_EVENTS
    CollectionSettings collection;
#endif
};
