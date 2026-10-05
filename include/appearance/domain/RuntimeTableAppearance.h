#ifndef RUNTIME_TABLE_APPEARANCE_H
#define RUNTIME_TABLE_APPEARANCE_H

#include <SdFat.h>
#include "appearance/ports/AppearanceLoader.h"
#include "pet_behavior/domain/PetBehaviorStatSlot.h"

// Each query streams and validates a fresh /runtime.bin snapshot. Appearance,
// Outfit and Evolution records stay on SD instead of occupying a resident table.
bool validateRuntimeTableAppearance(SdFat *sd,
                                    const AssetData::RuntimeManifest &manifest);
bool findRuntimeTableEvolutionTarget(SdFat *sd,
                                     const AssetData::RuntimeManifest &manifest,
                                     BundleReader &bundleReader,
                                     const ActivePetBehaviorStatSlots &activeSlots,
                                     const PetStatSnapshot &stats,
                                     AppearanceSelection &selection);
bool loadRuntimeTableSpecies(SdFat *sd, const AssetData::RuntimeManifest &manifest,
                             uint8_t *species,
                             size_t maxSpecies, size_t &speciesCount);
bool loadRuntimeTableOutfits(SdFat *sd, const AssetData::RuntimeManifest &manifest,
                             uint8_t speciesSlot,
                             uint8_t unlockMask, uint8_t *outfits, size_t maxOutfits, size_t &outfitCount);
bool findRuntimeTableOutfitPreview(SdFat *sd, const AssetData::RuntimeManifest &manifest,
                                   BundleReader &bundleReader, uint8_t speciesSlot,
                                   uint8_t outfitSlot, bool locked, OutfitPreview &preview);
bool resolveRuntimeTableOutfitUnlockMask(SdFat *sd, const AssetData::RuntimeManifest &manifest,
                                         uint8_t speciesSlot,
                                         const ActivePetBehaviorStatSlots &activeSlots,
                                         const PetStatSnapshot &stats, uint8_t currentMask, bool initialize,
                                         uint8_t &resolvedMask);
bool resolveRuntimeTableConsumableOutfitUnlock(SdFat *sd,
                                               const AssetData::RuntimeManifest &manifest,
                                               uint8_t speciesSlot, uint8_t outfitSlot,
                                               const ActivePetBehaviorStatSlots &activeSlots,
                                               const PetStatSnapshot &stats,
                                               PetStatSnapshot &consumedStats);

#endif // RUNTIME_TABLE_APPEARANCE_H
