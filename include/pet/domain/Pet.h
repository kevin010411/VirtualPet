#ifndef PET_H
#define PET_H

#include <Arduino.h>
#include "animation/domain/Animation.h"

template <typename T>
static inline T clampValue(T v, T lo, T hi) { return v < lo ? lo : (v > hi ? hi : v); }

struct PetConfig
{
    uint32_t maxAgeTenths = 1000;      // 100.0 歲，單位為 0.1 歲
    uint8_t max_hunger = 100;          // 0=不餓, 100=超餓
    uint8_t max_mood = 100;            // 0=極差, 100=極好
    unsigned int max_clean = 300;      // 0=極差, 300=極好
    unsigned int max_env_clean = 1000; // 0=極差, 1000=極好
    uint16_t ageTenthsPerTick = 2;     // 每 tick 增齡 0.2 歲
    uint8_t hungry_threshold = 70;
    uint8_t depressed_threshold = 30;
    unsigned int dirty_threshold = 100;
    unsigned int poop_threshold = 300;
};

struct PersistedPetState
{
    uint32_t magic;
    uint16_t version;
    uint32_t sequence;

    bool hasSick;
    uint8_t status;
    uint32_t ageTenths;

    int32_t hungry_value;
    int32_t mood;
    int32_t clean_value;
    int32_t env_value;
    char species[9];
    char outfit[9];
    uint32_t stage_days;
    int32_t health;
    int16_t customStats[8];
    uint32_t flowFlags;
    uint32_t crc32;
};

struct PetStatSnapshot
{
    static constexpr size_t kCustomStatCount = 8;

    uint32_t stage_days;
    int32_t health;
    char species[9];
    char outfit[9];
    int32_t age;
    int32_t hunger;
    int32_t mood;
    int32_t clean;
    int32_t env;
    int32_t sick;
    int32_t status;
    int16_t customStats[kCustomStatCount];
};

enum class HealthStatus
{
    Healthy,
    Hungry,
    Depressed,
    Dirty,
    Poop,
    Sick,
};

HealthStatus decide_state(uint8_t hunger, uint8_t mood, unsigned int env_value,
                          unsigned int clean_value, bool hasSick, const PetConfig &cfg);

class Pet
{
public:
    static constexpr uint32_t kAgeScale = 10;
    static constexpr uint32_t kPetStateMagic = 0x50455431;
    static constexpr uint16_t kPetStateVersion = 9;

    Pet(uint32_t ageTenths = 0);

    void dayPassed();
    void feedPet(int add_satiety);
    void changeMood(int delta);
    void takeShower(int value);
    void cleanEnv(unsigned int clear_value);
    void decayEnvironment(unsigned int decay_value);
    void getSick();
    bool takeMedicine();
    void setDefaultState();
    void setConfig(const PetConfig &newConfig);

    HealthStatus getStatus() const;
    bool isMoodDepressed() const;
    AnimationId CurrentAnimation() const;
    AnimationId CurrentAgeAnimation() const;
    uint16_t CurrentAgeFrame(uint16_t maxFrame) const;
    uint16_t CurrentMoodFrame(uint16_t maxFrame) const;
    uint16_t CurrentHungerFrame(uint16_t maxFrame) const;
    const char *speciesCode() const;
    const char *outfitCode() const;
    uint32_t stageDays() const;
    int32_t health() const;
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
    bool isCustomRulesInitialized() const;
    void markCustomRulesInitialized();

    const PersistedPetState &persistentState() const;
    bool restoreState(const PersistedPetState &state);

private:
    void refreshStatus();

    PetConfig cfg;
    PersistedPetState st = {};

};

#endif
