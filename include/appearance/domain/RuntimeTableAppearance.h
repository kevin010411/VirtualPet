#ifndef RUNTIME_TABLE_APPEARANCE_H
#define RUNTIME_TABLE_APPEARANCE_H

#include <SdFat.h>
#include "appearance/ports/AppearanceLoader.h"
#include "pet_behavior/domain/PetBehaviorStatSlot.h"

// Ticket 04 owns the binary Appearance and Evolution sections.  The functions
// intentionally stream /runtime.bin: no project-sized appearance table is
// retained in STM32 RAM during the expand-contract migration.
bool validateRuntimeTableAppearance(SdFat *sd,
                                    const AssetData::RuntimeManifest &manifest,
                                    BundleReader &bundleReader,
                                    const ActivePetBehaviorStatSlots &activeSlots,
                                    const PetStatSnapshot &stats);
bool loadRuntimeTableInitialAppearance(SdFat *sd,
                                       const AssetData::RuntimeManifest &manifest,
                                       BundleReader &bundleReader,
                                       AppearanceSelection &selection,
                                       AssetData::AnimationRef *idleAnimation = nullptr);
bool findRuntimeTableEvolutionTarget(SdFat *sd,
                                     const AssetData::RuntimeManifest &manifest,
                                     BundleReader &bundleReader,
                                     const ActivePetBehaviorStatSlots &activeSlots,
                                     const PetStatSnapshot &stats,
                                     AppearanceSelection &selection);
bool loadRuntimeTableSpecies(SdFat *sd, const AssetData::RuntimeManifest &manifest,
                             BundleReader &bundleReader, uint8_t *species,
                             size_t maxSpecies, size_t &speciesCount);
bool loadRuntimeTableOutfits(SdFat *sd, const AssetData::RuntimeManifest &manifest,
                             BundleReader &bundleReader, uint8_t speciesSlot,
                             uint8_t *outfits, size_t maxOutfits, size_t &outfitCount);
bool findRuntimeTableOutfitPreview(SdFat *sd, const AssetData::RuntimeManifest &manifest,
                                   BundleReader &bundleReader, uint8_t speciesSlot,
                                   uint8_t outfitSlot, OutfitPreview &preview);

#endif // RUNTIME_TABLE_APPEARANCE_H
