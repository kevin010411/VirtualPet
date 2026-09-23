#include <assert.h>
#include <string.h>

#include "appearance/ports/AppearanceLoader.h"
#include "pet_behavior/domain/RuntimeContractLoader.h"
#include "pet_behavior/domain/RuntimeTableBehavior.h"
#include "shared/assets/BundleReader.h"

namespace
{
int manifestReads = 0;
int bundleConfigurations = 0;
int completeTableReads = 0;
bool manifestSucceeds = true;
bool bundleSucceeds = true;
bool tableSucceeds = true;
bool initialQuerySucceeds = true;
const char *bundleError = "";
AssetData::RuntimeManifest observedManifest = {};

void reset()
{
    manifestReads = 0;
    bundleConfigurations = 0;
    completeTableReads = 0;
    manifestSucceeds = true;
    bundleSucceeds = true;
    tableSucceeds = true;
    initialQuerySucceeds = true;
    bundleError = "";
    observedManifest = {};
}
} // namespace

bool loadRuntimeManifest(SdFat *, AssetData::RuntimeManifest &manifest)
{
    ++manifestReads;
    manifest.fileSize = 42;
    return manifestSucceeds;
}

BundleReader::BundleReader(SdFat *, uint8_t *, size_t) {}

bool BundleReader::configureBundle(const AssetData::BundleId &)
{
    ++bundleConfigurations;
    return bundleSucceeds;
}

const char *BundleReader::firstErrorResource() const
{
    return bundleError;
}

bool loadCompleteRuntimeTable(SdFat *, const AssetData::RuntimeManifest &manifest,
                              BundleReader &, uint8_t speciesSlot, uint8_t outfitSlot,
                              PetBehaviorConfig &config,
                              AppearanceSelection *selection, bool *initialAppearanceResolved)
{
    ++completeTableReads;
    observedManifest = manifest;
    if (selection != nullptr)
    {
        if (!initialQuerySucceeds)
            return false;
        selection->speciesSlot = 1;
        selection->outfitSlot = 1;
        *initialAppearanceResolved = true;
        speciesSlot = 1;
        outfitSlot = 1;
    }
    config.activeSpeciesSlot = speciesSlot;
    config.activeOutfitSlot = outfitSlot;
    return tableSucceeds;
}

int main()
{
    SdFat sd;
    PetBehaviorConfig config = {};
    char error[20] = {};
    AssetData::RuntimeManifest startupManifest = {};
    startupManifest.fileSize = 99;

    // Startup uses the manifest already read for initial appearance.
    assert(loadRuntimeContract(&sd, 2, 3, config, error, sizeof(error), &startupManifest));
    assert(manifestReads == 0);
    assert(bundleConfigurations == 1 && completeTableReads == 1);
    assert(observedManifest.fileSize == 99);
    assert(config.activeSpeciesSlot == 2 && config.activeOutfitSlot == 3);
    assert(error[0] == '\0');

    reset();
    AppearanceSelection initial = {};
    bool initialResolved = false;
    assert(loadInitialRuntimeContract(&sd, startupManifest, initial, config,
                                      initialResolved, error, sizeof(error)));
    assert(initialResolved && initial.speciesSlot == 1 && initial.outfitSlot == 1);
    assert(manifestReads == 0 && bundleConfigurations == 1 && completeTableReads == 1);
    assert(observedManifest.fileSize == 99);

    reset();
    initialQuerySucceeds = false;
    assert(!loadInitialRuntimeContract(&sd, startupManifest, initial, config,
                                       initialResolved, error, sizeof(error)));
    assert(!initialResolved && strcmp(error, "runtime.bin") == 0);

    reset();
    tableSucceeds = false;
    assert(!loadInitialRuntimeContract(&sd, startupManifest, initial, config,
                                       initialResolved, error, sizeof(error)));
    assert(initialResolved && strcmp(error, "runtime.bin") == 0);

    // Later appearance changes still refresh the manifest.
    reset();
    assert(loadRuntimeContract(&sd, 4, 5, config, error, sizeof(error)));
    assert(manifestReads == 1);
    assert(observedManifest.fileSize == 42);
    assert(config.activeSpeciesSlot == 4 && config.activeOutfitSlot == 5);

    reset();
    manifestSucceeds = false;
    assert(!loadRuntimeContract(&sd, 4, 5, config, error, sizeof(error)));
    assert(strcmp(error, "runtime.bin") == 0);
    assert(bundleConfigurations == 0 && completeTableReads == 0);

    reset();
    bundleSucceeds = false;
    bundleError = "species1.pack";
    assert(!loadRuntimeContract(&sd, 2, 3, config, error, sizeof(error), &startupManifest));
    assert(strcmp(error, "species1.pack") == 0);
    assert(manifestReads == 0 && completeTableReads == 0);

    reset();
    tableSucceeds = false;
    assert(!loadRuntimeContract(&sd, 2, 3, config, error, sizeof(error), &startupManifest));
    assert(strcmp(error, "runtime.bin") == 0);
    assert(manifestReads == 0 && completeTableReads == 1);

    reset();
    assert(!loadRuntimeContract(&sd, 0, 3, config, error, sizeof(error), &startupManifest));
    assert(strcmp(error, "runtime.bin") == 0);
    assert(manifestReads == 0 && bundleConfigurations == 0);
}
