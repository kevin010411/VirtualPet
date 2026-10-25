#pragma once

#include "pet/PetBehaviorTypes.h"
#include "resources/detail/RuntimeTableReader.h"

namespace RuntimeTableInternal
{
// Decode into the caller's storage using the same open table snapshot.
// Behavior initializes config; presentation follows it and uses the decoded
// stat slots. A failed decode invalidates the in-progress configuration.
bool decodeRuntimeTableBehavior(const RuntimeTable &table,
                                const AssetData::RuntimeManifest &manifest,
                                uint8_t speciesSlot,
                                uint8_t outfitSlot,
                                PetBehaviorConfig &config);

bool decodeRuntimePresentation(const RuntimeTable &table,
                               uint8_t speciesSlot,
                               uint8_t outfitSlot,
                               PetBehaviorConfig &config);
} // namespace RuntimeTableInternal
