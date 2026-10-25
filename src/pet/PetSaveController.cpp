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

PetSaveResult PetSaveController::requestSave()
{
    return saveCollectionProgress(true);
}

PetSaveResult PetSaveController::saveCollectionProgress(bool forceSave, bool immediate)
{
    if (pet.isDead()) return saveSnapshot(true);
#if ENABLE_COLLECTION_CATALOG
    // Evaluate against the complete mutation before committing one A/B snapshot.
    // Appearance, effects, counters, acquisition and unlock bits travel together.
    bool changed = false;
    if (config != nullptr && !evaluateCollectionCatalog(renderer.sdCard(), *config, pet, changed))
    {
        renderer.recordAssetDataErrorResource("runtime.bin");
        return PetSaveResult::Failed;
    }
    if (!forceSave && !changed) return PetSaveResult::NotNeeded;
#else
    if (!forceSave) return PetSaveResult::NotNeeded;
#endif
    return saveSnapshot(immediate);
}

PetSaveResult PetSaveController::saveSnapshot(bool immediate)
{
    if (batching && !immediate)
    {
        batchDirty = true;
        return PetSaveResult::Deferred;
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
    return saved ? PetSaveResult::Persisted : PetSaveResult::Failed;
}

PetSaveResult PetSaveController::saveDailyProgress(bool forceSave)
{
    // One call per completed two-second pet day. Evaluate acquisitions every
    // day, but ordinary progress writes only after two days (four seconds).
    if (saveCounter < savePeriodTicks) ++saveCounter;
    if (batching) batchDirty = true;
    const PetSaveResult result = saveCollectionProgress(forceSave || saveCounter >= savePeriodTicks);
    return batching && result == PetSaveResult::NotNeeded ? PetSaveResult::Deferred : result;
}

PetSaveResult PetSaveController::maybeSave()
{
    if (saveCounter < savePeriodTicks) ++saveCounter;
    if (batching)
    {
        batchDirty = true;
        return PetSaveResult::Deferred;
    }
    if (saveCounter < savePeriodTicks)
        return PetSaveResult::NotNeeded;

    return requestSave();
}

bool PetSaveController::applyCollectionGrants()
{
#if ENABLE_COLLECTION_CATALOG
    return config == nullptr || applyCollectionOutfitGrants(renderer.sdCard(), *config, pet);
#else
    return true;
#endif
}

PetSaveResult PetSaveController::endBatch()
{
    const bool flush = batching && batchDirty;
    batching = false;
    return flush ? saveSnapshot() : PetSaveResult::NotNeeded;
}
