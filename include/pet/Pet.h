#ifndef PET_H
#define PET_H

#include <Arduino.h>
#include "common/AppProfile.h"

static constexpr size_t kPetCustomStatCount = 12;
static constexpr size_t kTrackedActionCount = 8;

struct PersistedPetState
{
    uint32_t magic;
    uint16_t version;
    uint32_t sequence;
    uint32_t schemaFingerprint;
    uint32_t stage_days;
    uint32_t flowFlags;
    uint8_t outfitUnlockMask;
    uint32_t customStats[kPetCustomStatCount];
    char speciesSlotText[9];
    char outfitSlotText[9];
    uint32_t collectionFingerprint;
    uint64_t collectionMask;
    uint32_t trackedActionFingerprint;
    uint16_t actionCounts[kTrackedActionCount];
    uint32_t crc32;
};

struct PetStatSnapshot
{
    static constexpr size_t kCustomStatCount = kPetCustomStatCount;

    uint32_t stage_days;
    uint8_t speciesSlot;
    uint8_t outfitSlot;
    uint32_t customStats[kCustomStatCount];
};

class Pet
{
public:
    static constexpr uint32_t kPetStateMagic = 0x50455431;
    static constexpr uint16_t kPetStateVersion = 19;
    static constexpr uint32_t kStageDaysMax = 864000UL;

    Pet();

    void setDefaultState();
    bool isDead() const;
#if ENABLE_DEATH
    void configureDeath(bool enabled, uint16_t statMask);
    bool evaluateDeath();
#else
    void configureDeath(bool, uint16_t) {}
    // Saved Dead still blocks care until the existing new-life reset succeeds.
    bool evaluateDeath() { return isDead(); }
#endif
    void setSchemaFingerprint(uint32_t fingerprint);

    uint8_t speciesSlot() const;
    uint8_t outfitSlot() const;
    uint32_t stageDays() const;
    void setStageDays(uint32_t value);
    bool commitPetStats(const uint32_t *customStats, size_t customStatCount);
    bool commitPetDay(const uint32_t *customStats, size_t customStatCount, uint32_t days = 1);
    PetStatSnapshot statSnapshot() const;
    uint32_t customStat(uint8_t index) const;
    bool setCustomStat(uint8_t index, uint32_t value);
    bool setSpeciesSlot(uint8_t slot);
    bool setOutfitSlot(uint8_t slot);
    uint8_t outfitUnlockMask() const;
    bool isOutfitUnlocked(uint8_t slot) const;
    void unlockOutfit(uint8_t slot);
    void initializeOutfitUnlockMask(uint8_t mask);
    bool commitConsumableOutfitUnlock(uint8_t outfitSlot,
                                      const PetStatSnapshot &consumedStats);
    bool isFirstLaunchComplete() const;
    bool isFirstStartCompleted() const;
    void markFirstStartCompleted();
    void resetFirstStartCompleted();
    const PersistedPetState &persistentState() const;
    bool restoreState(const PersistedPetState &state);
    bool configureCollection(uint32_t fingerprint);
    bool isCollectionOwned(uint8_t slot) const;
    void acquireCollection(uint8_t slot);
    bool configureTrackedActions(uint32_t fingerprint);
    uint16_t actionCount(uint8_t slot) const;
    void incrementActionCount(uint8_t slot);
    void resetActionCounts();

private:
    PersistedPetState st = {};
#if ENABLE_DEATH
    uint16_t deathStatMask = 0;
#endif

};

#endif
