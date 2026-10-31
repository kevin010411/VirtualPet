#ifndef PET_SAVE_CONTROLLER_H
#define PET_SAVE_CONTROLLER_H

#include <stdint.h>
#include "common/AppProfile.h"

class Pet;
class PetStorage;
class AssetResources;
#if ENABLE_DEBUG
class DebugDisplay;
#endif
struct PetBehaviorConfig;

enum class PetSaveResult : uint8_t { NotNeeded, Deferred, Persisted, Failed };

class PetSaveController
{
public:
    PetSaveController(Pet &pet, PetStorage &storage, AssetResources &resources
#if ENABLE_DEBUG
                      , DebugDisplay &debug
#endif
    );
    // Requests participate in an active batch; only Persisted confirms a write.
    PetSaveResult requestSave();
    PetSaveResult saveLifeState() { return saveSnapshot(true); }
    void resetTransientState() { saveCounter = 0; batching = batchDirty = false; }
    // Immediate event commits bypass a periodic RTC catch-up save batch.
    PetSaveResult saveCollectionProgress(bool forceSave, bool immediate = false);
    PetSaveResult saveDailyProgress(bool forceSave = false);
    bool applyCollectionGrants();
    void configureRuntimeContract(const PetBehaviorConfig &value) { config = &value; }
    PetSaveResult maybeSave();
    void beginBatch() { batching = true; }
    PetSaveResult endBatch();

private:
    static constexpr uint8_t savePeriodTicks = 2;
    Pet &pet;
    PetStorage &petStorage;
    AssetResources &resources;
#if ENABLE_DEBUG
    DebugDisplay &debug;
#endif
    uint8_t saveCounter = 0;
    const PetBehaviorConfig *config = nullptr;
    PetSaveResult saveSnapshot(bool immediate = false);
    bool batching = false;
    bool batchDirty = false;
};

#endif
