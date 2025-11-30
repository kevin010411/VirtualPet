#include "Pet.h"

enum class HealthStatus
{
    Healthy,
    Hungry,
    Depressed,
    Dirty,
    Poop,
    Sick,
};

HealthStatus decide_state(
    uint8_t hunger, uint8_t mood, unsigned int env_value, unsigned int clean_value, bool hasSick, const PetConfig &cfg)
{
    // 優先權：Sick > Hungry > Depressed > Poop >  Dirty > Healthy
    if (hasSick)
        return HealthStatus::Sick;
    if (hunger >= 70)
        return HealthStatus::Hungry;
    if (mood <= 30)
        return HealthStatus::Depressed;
    if (env_value <= 300)
        return HealthStatus::Poop;
    if (clean_value <= 100)
        return HealthStatus::Dirty;
    return HealthStatus::Healthy;
}

Pet::Pet(Adafruit_ST7735 *ref_tft, float age) : age(age), hungry_value(50), mood(50),
                                                env_value(1000), clean_value(300), hasSick(false),
                                                tft(ref_tft), status(HealthStatus::Healthy) {}

void Pet::dayPassed()
{
    hungry_value = clamp<int>(hungry_value + 1, 0, cfg.max_hunger);
    mood = clamp<int>(mood - 1, 0, cfg.max_mood);
    age = clamp<float>(age + cfg.age_per_tick, 0.0f, cfg.max_age);
    clean_value = clamp<int>(clean_value - 1, 0, cfg.max_clean);
    env_value = clamp<int>(env_value - 1, 0, cfg.max_env_clean);

    status = decide_state(hungry_value, mood, env_value, clean_value, hasSick, cfg);
}

void Pet::feedPet(int add_satiety)
{
    hungry_value = clamp<int>(hungry_value - add_satiety, 0, cfg.max_hunger);
}

void Pet::changeMood(int delta)
{
    mood = clamp<int>(mood + delta, 0, cfg.max_mood);
}

void Pet::takeShower(int value)
{
    clean_value = clamp<int>(clean_value + value, 0, cfg.max_clean);
}

void Pet::cleanEnv(unsigned int clear_value)
{
    env_value = clamp<int>(env_value + clear_value, 0, cfg.max_env_clean);
}

void Pet::getSick()
{
    hasSick = true;
}

bool Pet::takeMedicine()
{
    if (!hasSick)
        return false;
    hasSick = false;
    return true;
}

String Pet::CurrentAnimation() const
{
    switch (status)
    {
    case HealthStatus::Healthy:
        return "idle"; // 平時
    case HealthStatus::Hungry:
        return "hungry"; // 肚子餓
    case HealthStatus::Depressed:
        return "depress"; // 心情差
    case HealthStatus::Sick:
        return "sick"; // 生病
    case HealthStatus::Dirty:
        return "dirty"; // 臭臭
    case HealthStatus::Poop:
        return "poop"; // 大便
    }
    return "idle";
}