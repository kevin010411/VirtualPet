#include "domain/Pet.h"

#include <string.h>

namespace
{
bool copyAppearanceCode(char *dest, size_t destSize, const char *source)
{
    if (dest == nullptr || destSize == 0 || source == nullptr || source[0] == '\0')
        return false;

    const size_t len = strlen(source);
    if (len >= destSize)
        return false;

    for (size_t i = 0; i < len; ++i)
    {
        const char c = source[i];
        const bool valid = (c >= 'a' && c <= 'z') ||
                           (c >= 'A' && c <= 'Z') ||
                           (c >= '0' && c <= '9') ||
                           c == '_' || c == '-';
        if (!valid)
            return false;
    }

    strcpy(dest, source);
    return true;
}
} // namespace

HealthStatus decide_state(
    uint8_t hunger, uint8_t mood, unsigned int env_value, unsigned int clean_value, bool hasSick, const PetConfig &cfg)
{
    if (hasSick)
        return HealthStatus::Sick;
    if (hunger >= cfg.hungry_threshold)
        return HealthStatus::Hungry;
    if (mood <= cfg.depressed_threshold)
        return HealthStatus::Depressed;
    if (env_value <= cfg.poop_threshold)
        return HealthStatus::Poop;
    if (clean_value <= cfg.dirty_threshold)
        return HealthStatus::Dirty;
    return HealthStatus::Healthy;
}

Pet::Pet(float age)
{
    setDefaultState();
    st.age = clampValue<float>(age, 0.0f, cfg.max_age);
    refreshStatus();
}

void Pet::setConfig(const PetConfig &newConfig)
{
    cfg = newConfig;
    refreshStatus();
}

void Pet::refreshStatus()
{
    st.status = static_cast<uint8_t>(decide_state(
        st.hungry_value,
        st.mood,
        st.env_value,
        st.clean_value,
        st.hasSick,
        cfg));
}

bool Pet::dayPassed()
{
    if (getStatus() != HealthStatus::Healthy)
        return false;

    st.hungry_value = clampValue<int>(st.hungry_value + 3, 0, cfg.max_hunger);
    st.mood = clampValue<int>(st.mood - 3, 0, cfg.max_mood);
    st.age = clampValue<float>(st.age + cfg.age_per_tick, 0.0f, cfg.max_age);
    st.clean_value = clampValue<int>(st.clean_value - 3, 0, cfg.max_clean);
    st.env_value = clampValue<int>(st.env_value - 3, 0, cfg.max_env_clean);
    st.healthy_days += 1;
    refreshStatus();
    return true;
}

void Pet::feedPet(int add_satiety)
{
    st.hungry_value = clampValue<int>(st.hungry_value - add_satiety, 0, cfg.max_hunger);
    refreshStatus();
}

void Pet::changeMood(int delta)
{
    st.mood = clampValue<int>(st.mood + delta, 0, cfg.max_mood);
    refreshStatus();
}

void Pet::takeShower(int value)
{
    st.clean_value = clampValue<int>(st.clean_value + value, 0, cfg.max_clean);
    refreshStatus();
}

void Pet::cleanEnv(unsigned int clear_value)
{
    st.env_value = clampValue<int>(st.env_value + clear_value, 0, cfg.max_env_clean);
    refreshStatus();
}

void Pet::decayEnvironment(unsigned int decay_value)
{
    st.env_value = clampValue<int>(st.env_value - static_cast<int>(decay_value), 0, cfg.max_env_clean);
    refreshStatus();
}

void Pet::getSick()
{
    if (getStatus() != HealthStatus::Healthy)
        return;

    st.hasSick = true;
    refreshStatus();
}

bool Pet::takeMedicine()
{
    if (!st.hasSick)
        return false;

    st.hasSick = false;
    refreshStatus();
    return true;
}

AnimationId Pet::CurrentAnimation() const
{
    switch (getStatus())
    {
    case HealthStatus::Healthy:
        return AnimationId::Idle;
    case HealthStatus::Hungry:
        return AnimationId::Hungry;
    case HealthStatus::Depressed:
        return AnimationId::Depress;
    case HealthStatus::Sick:
        return AnimationId::Sick;
    case HealthStatus::Dirty:
        return AnimationId::Dirty;
    case HealthStatus::Poop:
        return AnimationId::Poop;
    }

    return AnimationId::Idle;
}

AnimationId Pet::CurrentAgeAnimation() const
{
    return AnimationId::StatusAge;
}

uint16_t Pet::CurrentAgeFrame(uint16_t maxFrame) const
{
    if (maxFrame <= 1)
        return 1;

    const float normalized = cfg.max_age <= 0.0f ? 1.0f : clampValue<float>(st.age / cfg.max_age, 0.0f, 1.0f);
    const uint16_t frame = static_cast<uint16_t>(normalized * static_cast<float>(maxFrame)) + 1;
    return clampValue<uint16_t>(frame, 1, maxFrame);
}

uint16_t Pet::CurrentMoodFrame(uint16_t maxFrame) const
{
    if (maxFrame <= 1)
        return 1;

    const float normalized = cfg.max_mood == 0 ? 1.0f : clampValue<float>(static_cast<float>(st.mood) / static_cast<float>(cfg.max_mood), 0.0f, 1.0f);
    const uint16_t frame = static_cast<uint16_t>(normalized * static_cast<float>(maxFrame)) + 1;
    return clampValue<uint16_t>(frame, 1, maxFrame);
}

uint16_t Pet::CurrentHungerFrame(uint16_t maxFrame) const
{
    if (maxFrame <= 1)
        return 1;

    const float normalized = cfg.max_hunger == 0 ? 1.0f : clampValue<float>(static_cast<float>(st.hungry_value) / static_cast<float>(cfg.max_hunger), 0.0f, 1.0f);
    const uint16_t frame = static_cast<uint16_t>(normalized * static_cast<float>(maxFrame)) + 1;
    return clampValue<uint16_t>(frame, 1, maxFrame);
}

void Pet::setDefaultState()
{
    st.magic = kPetStateMagic;
    st.version = kPetStateVersion;
    st.sequence = 0;
    st.hasSick = false;
    st.status = static_cast<uint8_t>(HealthStatus::Healthy);
    st.age = 0.0f;
    st.hungry_value = 0;
    st.mood = 70;
    st.clean_value = 200;
    st.env_value = 800;
    strcpy(st.species, "dino");
    strcpy(st.outfit, "base");
    st.healthy_days = 0;
    st.crc32 = 0;
}

String Pet::getAge()
{
    switch (CurrentAgeFrame(3))
    {
    case 1:
        return "0.1";
    case 2:
        return "0.5";
    default:
        return "1";
    }
}

HealthStatus Pet::getStatus() const
{
    return static_cast<HealthStatus>(st.status);
}

const PersistedPetState &Pet::persistentState() const
{
    return st;
}

const char *Pet::speciesCode() const
{
    return st.species[0] == '\0' ? "dino" : st.species;
}

const char *Pet::outfitCode() const
{
    return st.outfit[0] == '\0' ? "base" : st.outfit;
}

uint32_t Pet::healthyDays() const
{
    return st.healthy_days;
}

bool Pet::setSpeciesCode(const char *code)
{
    return copyAppearanceCode(st.species, sizeof(st.species), code);
}

bool Pet::setOutfitCode(const char *code)
{
    return copyAppearanceCode(st.outfit, sizeof(st.outfit), code);
}

bool Pet::restoreState(const PersistedPetState &state)
{
    if (state.magic != kPetStateMagic || state.version != kPetStateVersion)
        return false;

    st = state;
    st.version = kPetStateVersion;
    st.age = clampValue<float>(st.age, 0.0f, cfg.max_age);
    st.hungry_value = clampValue<int32_t>(st.hungry_value, 0, cfg.max_hunger);
    st.mood = clampValue<int32_t>(st.mood, 0, cfg.max_mood);
    st.clean_value = clampValue<int32_t>(st.clean_value, 0, cfg.max_clean);
    st.env_value = clampValue<int32_t>(st.env_value, 0, cfg.max_env_clean);

    char appearanceCode[9] = {};
    if (!copyAppearanceCode(appearanceCode, sizeof(appearanceCode), st.species))
        strcpy(st.species, "dino");
    if (!copyAppearanceCode(appearanceCode, sizeof(appearanceCode), st.outfit))
        strcpy(st.outfit, "base");

    refreshStatus();
    return true;
}
