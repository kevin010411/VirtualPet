#pragma once

#include "resources/RuntimeContract.h"
#include "resources/detail/RuntimeTableReader.h"

namespace RuntimeTableInternal
{
// Decode into the caller's storage using the same open table snapshot.
// Caller zero-initializes config; presentation follows behavior and uses the decoded
// stat slots. A failed decode invalidates the in-progress configuration.
bool decodeRuntimeTableBehavior(const RuntimeTable &table,
                                const AssetData::RuntimeManifest &manifest,
                                uint8_t speciesSlot,
                                uint8_t outfitSlot,
                                RuntimeContract &config);

bool decodeRuntimePresentation(const RuntimeTable &table,
                               uint8_t speciesSlot,
                               uint8_t outfitSlot,
                               RuntimeContract &config);
} // namespace RuntimeTableInternal
