#ifndef PET_H
#define PET_H

#include <Arduino.h>
#include "Animation.h"

template <typename T>
static inline T clamp(T v, T lo, T hi) { return v < lo ? lo : (v > hi ? hi : v); }

struct PetConfig
{
    float max_age = 100;               // 歲
    uint8_t max_hunger = 100;          // 0=不餓, 100=超餓
    uint8_t max_mood = 100;            // 0=極差, 100=極好
    unsigned int max_clean = 300;      // 0=極差, 300=極好
    unsigned int max_env_clean = 1000; // 0=極差, 1000=極好
    float age_per_tick = 0.2f;         // 每 tick 增齡
    uint16_t min_stay_ticks = 10;      // 任一狀態最短停留（避免抖動）
    uint8_t hungry_threshold = 70;
    uint8_t depressed_threshold = 30;
    unsigned int dirty_threshold = 100;
    unsigned int poop_threshold = 300;
};

struct PersisState
{
    uint32_t magic;
    uint16_t version;

    bool hasSick;
    uint8_t status;
    float age;

    int32_t hungry_value;
    int32_t mood;
    int32_t clean_value;
    int32_t env_value;
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
    AnimationId CurrentAnimation() const;
    AnimationId CurrentAgeAnimation() const;
    String getAge();

    const PersisState &persistentState() const;
    bool restoreState(const PersisState &state);

private:
    void refreshStatus();

    PetConfig cfg;
    PersisState st = {};

    static constexpr uint32_t MAGIC = 0x50455431;
    static constexpr uint16_t VER = 1;
};

#endif
