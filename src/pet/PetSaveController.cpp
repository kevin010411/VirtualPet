#include "pet/PetSaveController.h"

#include <stdio.h>
#include "pet/Pet.h"
#include "display/Renderer.h"
#include "pet/PetStorage.h"
#include "common/AppProfile.h"
#if ENABLE_COLLECTION_CATALOG
#include "collection/CollectionCatalog.h"
#endif

PetSaveController::PetSaveController(Pet &petRef, PetStorage &petStorageRef, Renderer &rendererRef)
    : pet(petRef),
      petStorage(petStorageRef),
      renderer(rendererRef)
{
}

bool PetSaveController::saveNow()
{
    return saveCollectionProgress(true);
}

bool PetSaveController::saveCollectionProgress(bool forceSave, bool immediate)
{
#if ENABLE_COLLECTION_CATALOG
    // Evaluate against the complete mutation before committing one A/B snapshot.
    // Appearance, effects, counters, acquisition and unlock bits travel together.
    bool changed = false;
    if (config != nullptr && !evaluateCollectionCatalog(renderer.sdCard(), *config, pet, changed))
    {
        renderer.recordAssetDataErrorResource("runtime.bin");
        return false;
    }
    if (!forceSave && !changed) return true;
#else
    if (!forceSave) return true;
#endif
    return saveSnapshot(immediate);
}

bool PetSaveController::saveSnapshot(bool immediate)
{
    if (batching && !immediate)
    {
        batchDirty = true;
        return true;
    }
    const bool saved = petStorage.save(pet);
#if ENABLE_DEBUG
    char title[21] = {};
    char detail[21] = {};
    snprintf(
        title,
        sizeof(title),
        "SAVE %s %c %lu",
        saved ? "OK" : "FAIL",
        petStorage.lastSaveSlot(),
        static_cast<unsigned long>(petStorage.lastSaveSequence()));
    snprintf(detail, sizeof(detail), "%u/%u", pet.speciesSlot(), pet.outfitSlot());
    renderer.debugDisplay().showMessage(title, detail);
#endif
    if (saved)
    {
        saveCounter = 0;
        batchDirty = false;
    }
    return saved;
}

bool PetSaveController::saveDailyProgress(bool forceSave)
{
    // One call per completed two-second pet day. Evaluate acquisitions every
    // day, but ordinary progress writes only after two days (four seconds).
    if (saveCounter < savePeriodTicks) ++saveCounter;
    if (batching) batchDirty = true;
    return saveCollectionProgress(forceSave || saveCounter >= savePeriodTicks);
}

void PetSaveController::maybeSave()
{
    if (saveCounter < savePeriodTicks) ++saveCounter;
    if (batching)
    {
        batchDirty = true;
        return;
    }
    if (saveCounter < savePeriodTicks)
        return;

    saveNow();
}

bool PetSaveController::applyCollectionGrants()
{
#if ENABLE_COLLECTION_CATALOG
    return config == nullptr || applyCollectionOutfitGrants(renderer.sdCard(), *config, pet);
#else
    return true;
#endif
}

void PetSaveController::endBatch()
{
    const bool flush = batching && batchDirty;
    batching = false;
    if (flush) saveSnapshot();
}
