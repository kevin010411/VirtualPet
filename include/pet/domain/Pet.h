#ifndef PET_H
#define PET_H

#include <Arduino.h>

template <typename T>
static inline T clampValue(T v, T lo, T hi) { return v < lo ? lo : (v > hi ? hi : v); }

static constexpr size_t kPetCustomStatCount = 8;

struct PersistedPetState
{
    uint32_t magic;
    uint16_t version;
    uint32_t sequence;
    uint32_t schemaFingerprint;
    uint32_t stage_days;
    uint32_t flowFlags;
    int16_t customStats[kPetCustomStatCount];
    char species[9];
    char outfit[9];
    uint32_t crc32;
};

struct PetStatSnapshot
{
    static constexpr size_t kCustomStatCount = kPetCustomStatCount;

    uint32_t stage_days;
    char species[9];
    char outfit[9];
    int16_t customStats[kCustomStatCount];
};

class Pet
{
public:
    static constexpr uint32_t kPetStateMagic = 0x50455431;
    static constexpr uint16_t kPetStateVersion = 12;

    Pet();

    void setDefaultState();
    void setSchemaFingerprint(uint32_t fingerprint);

    const char *speciesCode() const;
    const char *outfitCode() const;
    uint32_t stageDays() const;
    bool commitPetStats(const int16_t *customStats, size_t customStatCount);
    bool commitPetDay(const int16_t *customStats, size_t customStatCount);
    PetStatSnapshot statSnapshot() const;
    int16_t customStat(uint8_t index) const;
    bool setCustomStat(uint8_t index, int16_t value);
    bool changeCustomStat(uint8_t index, int16_t delta);
    bool changeCustomStatClamped(uint8_t index, int16_t delta, int16_t minValue, int16_t maxValue);
    bool setSpeciesCode(const char *code);
    bool setOutfitCode(const char *code);
    bool isFirstLaunchComplete() const;
    void markFirstLaunchComplete();
    void resetFirstLaunch();
    const PersistedPetState &persistentState() const;
    bool restoreState(const PersistedPetState &state);

private:
    PersistedPetState st = {};

};

#endif
