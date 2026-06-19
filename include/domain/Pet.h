#ifndef PET_H
#define PET_H

#include <Arduino.h>
#include "domain/Animation.h"

template <typename T>
static inline T clampValue(T v, T lo, T hi) { return v < lo ? lo : (v > hi ? hi : v); }

struct PetConfig
{
    float max_age = 100;               // 歲
    uint8_t max_hunger = 100;          // 0=不餓, 100=超餓
    uint8_t max_mood = 100;            // 0=極差, 100=極好
    unsigned int max_clean = 300;      // 0=極差, 300=極好
    unsigned int max_env_clean = 1000; // 0=極差, 1000=極好
    float age_per_tick = 0.2f;         // 每 tick 增齡
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
    float age;

    int32_t hungry_value;
    int32_t mood;
    int32_t clean_value;
    int32_t env_value;
    char species[9];
    char outfit[9];
    uint32_t healthy_days;
    uint32_t crc32;
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
    Pet(float age = 0);

    bool dayPassed();
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
    AnimationId CurrentAnimation() const;
    AnimationId CurrentAgeAnimation() const;
    uint16_t CurrentAgeFrame(uint16_t maxFrame) const;
    uint16_t CurrentMoodFrame(uint16_t maxFrame) const;
    uint16_t CurrentHungerFrame(uint16_t maxFrame) const;
    String getAge();
    const char *speciesCode() const;
    const char *outfitCode() const;
    uint32_t healthyDays() const;
    bool setSpeciesCode(const char *code);
    bool setOutfitCode(const char *code);

    const PersistedPetState &persistentState() const;
    bool restoreState(const PersistedPetState &state);

private:
    void refreshStatus();

    PetConfig cfg;
    PersistedPetState st = {};

    static constexpr uint32_t kPetStateMagic = 0x50455431;
    static constexpr uint16_t kPetStateVersion = 4;
};

#endif
