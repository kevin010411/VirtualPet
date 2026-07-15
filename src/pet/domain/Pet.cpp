#include "pet/domain/Pet.h"

#include <string.h>
#include "shared/config/AppProfile.h"

namespace
{
constexpr uint32_t kFirstLaunchCompleteFlag = 0x1UL;
constexpr uint32_t kCustomRulesInitializedFlag = 0x2UL;

uint16_t frameForRatio(uint32_t value, uint32_t maximum, uint16_t maxFrame)
{
    if (maxFrame <= 1)
        return 1;
    if (maximum == 0)
        return maxFrame;

    const uint32_t clampedValue = value > maximum ? maximum : value;
    const uint16_t frame = static_cast<uint16_t>((clampedValue * maxFrame) / maximum + 1U);
    return clampValue<uint16_t>(frame, 1, maxFrame);
}

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

Pet::Pet(uint32_t ageTenths)
{
    setDefaultState();
    st.ageTenths = clampValue<uint32_t>(ageTenths, 0, cfg.maxAgeTenths);
}

void Pet::setConfig(const PetConfig &newConfig)
{
    cfg = newConfig;
    st.ageTenths = clampValue<uint32_t>(st.ageTenths, 0, cfg.maxAgeTenths);
}

void Pet::dayPassed()
{
    const bool healthy = getStatus() == HealthStatus::Healthy;
    st.stage_days += 1;
    st.health = clampValue<int32_t>(st.health + (healthy ? 5 : -5), 0, 100);

    if (!healthy)
    {
#if APP_STATUS_MODE == STATUS_MODE_COMPOSITE
        st.mood = clampValue<int>(st.mood - 2, 0, cfg.max_mood);
#endif
        return;
    }

    st.hungry_value = clampValue<int>(st.hungry_value + 3, 0, cfg.max_hunger);
    st.mood = clampValue<int>(st.mood - 2, 0, cfg.max_mood);
    st.ageTenths = clampValue<uint32_t>(st.ageTenths + cfg.ageTenthsPerTick, 0, cfg.maxAgeTenths);
    st.clean_value = clampValue<int>(st.clean_value - 3, 0, cfg.max_clean);
    st.env_value = clampValue<int>(st.env_value - 3, 0, cfg.max_env_clean);
}

void Pet::feedPet(int add_satiety)
{
    st.hungry_value = clampValue<int>(st.hungry_value - add_satiety, 0, cfg.max_hunger);
}

void Pet::changeMood(int delta)
{
    st.mood = clampValue<int>(st.mood + delta, 0, cfg.max_mood);
}

void Pet::takeShower(int value)
{
    st.clean_value = clampValue<int>(st.clean_value + value, 0, cfg.max_clean);
}

void Pet::cleanEnv(unsigned int clear_value)
{
    st.env_value = clampValue<int>(st.env_value + clear_value, 0, cfg.max_env_clean);
}

void Pet::decayEnvironment(unsigned int decay_value)
{
    st.env_value = clampValue<int>(st.env_value - static_cast<int>(decay_value), 0, cfg.max_env_clean);
}

void Pet::getSick()
{
    if (getStatus() != HealthStatus::Healthy)
        return;

    st.hasSick = true;
}

bool Pet::takeMedicine()
{
    if (!st.hasSick)
        return false;

    st.hasSick = false;
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
    return frameForRatio(st.ageTenths, cfg.maxAgeTenths, maxFrame);
}

uint16_t Pet::CurrentMoodFrame(uint16_t maxFrame) const
{
    return frameForRatio(static_cast<uint32_t>(st.mood), cfg.max_mood, maxFrame);
}

uint16_t Pet::CurrentHungerFrame(uint16_t maxFrame) const
{
    return frameForRatio(static_cast<uint32_t>(st.hungry_value), cfg.max_hunger, maxFrame);
}

void Pet::setDefaultState()
{
    st = {};
    st.magic = kPetStateMagic;
    st.version = kPetStateVersion;
    st.sequence = 0;
    st.hasSick = false;
    st.ageTenths = 0;
    st.hungry_value = 0;
    st.mood = 70;
    st.clean_value = 200;
    st.env_value = 800;
    strcpy(st.species, "dino");
    strcpy(st.outfit, "base");
    st.stage_days = 0;
    st.health = 0;
    for (size_t i = 0; i < PetStatSnapshot::kCustomStatCount; ++i)
        st.customStats[i] = 0;
    st.flowFlags = 0;
    st.crc32 = 0;
}

HealthStatus Pet::getStatus() const
{
    return decide_state(
        static_cast<uint8_t>(st.hungry_value),
        static_cast<uint8_t>(st.mood),
        static_cast<unsigned int>(st.env_value),
        static_cast<unsigned int>(st.clean_value),
        st.hasSick,
        cfg);
}

bool Pet::isMoodDepressed() const
{
    return st.mood <= cfg.depressed_threshold;
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

uint32_t Pet::stageDays() const
{
    return st.stage_days;
}

int32_t Pet::health() const
{
    return st.health;
}

PetStatSnapshot Pet::statSnapshot() const
{
    PetStatSnapshot snapshot = {};
    snapshot.stage_days = st.stage_days;
    snapshot.health = st.health;
    strncpy(snapshot.species, speciesCode(), sizeof(snapshot.species) - 1);
    snapshot.species[sizeof(snapshot.species) - 1] = '\0';
    strncpy(snapshot.outfit, outfitCode(), sizeof(snapshot.outfit) - 1);
    snapshot.outfit[sizeof(snapshot.outfit) - 1] = '\0';
    snapshot.age = static_cast<int32_t>(st.ageTenths / kAgeScale);
    snapshot.hunger = st.hungry_value;
    snapshot.mood = st.mood;
    snapshot.clean = st.clean_value;
    snapshot.env = st.env_value;
    snapshot.sick = st.hasSick ? 1 : 0;
    snapshot.status = static_cast<int32_t>(getStatus());
    for (size_t i = 0; i < PetStatSnapshot::kCustomStatCount; ++i)
        snapshot.customStats[i] = st.customStats[i];
    return snapshot;
}

int16_t Pet::customStat(uint8_t index) const
{
    if (index >= PetStatSnapshot::kCustomStatCount)
        return 0;

    return st.customStats[index];
}

bool Pet::setCustomStat(uint8_t index, int16_t value)
{
    if (index >= PetStatSnapshot::kCustomStatCount)
        return false;

    st.customStats[index] = value;
    return true;
}

bool Pet::changeCustomStat(uint8_t index, int16_t delta)
{
    if (index >= PetStatSnapshot::kCustomStatCount)
        return false;

    st.customStats[index] = static_cast<int16_t>(st.customStats[index] + delta);
    return true;
}

bool Pet::changeCustomStatClamped(uint8_t index, int16_t delta, int16_t minValue, int16_t maxValue)
{
    if (index >= PetStatSnapshot::kCustomStatCount || minValue > maxValue)
        return false;

    const int32_t next = static_cast<int32_t>(st.customStats[index]) + delta;
    st.customStats[index] = static_cast<int16_t>(clampValue<int32_t>(next, minValue, maxValue));
    return true;
}

bool Pet::setSpeciesCode(const char *code)
{
    char nextSpecies[sizeof(st.species)] = {};
    if (!copyAppearanceCode(nextSpecies, sizeof(nextSpecies), code))
        return false;

    if (strcmp(st.species, nextSpecies) != 0)
    {
        strcpy(st.species, nextSpecies);
        st.stage_days = 0;
    }
    return true;
}

bool Pet::setOutfitCode(const char *code)
{
    return copyAppearanceCode(st.outfit, sizeof(st.outfit), code);
}

bool Pet::isFirstLaunchComplete() const
{
    return (st.flowFlags & kFirstLaunchCompleteFlag) != 0;
}

void Pet::markFirstLaunchComplete()
{
    st.flowFlags |= kFirstLaunchCompleteFlag;
}

void Pet::resetFirstLaunch()
{
    st.flowFlags &= ~kFirstLaunchCompleteFlag;
}

bool Pet::isCustomRulesInitialized() const
{
    return (st.flowFlags & kCustomRulesInitializedFlag) != 0;
}

void Pet::markCustomRulesInitialized()
{
    st.flowFlags |= kCustomRulesInitializedFlag;
}

bool Pet::restoreState(const PersistedPetState &state)
{
    if (state.magic != kPetStateMagic || state.version != kPetStateVersion)
        return false;

    st = state;
    st.version = kPetStateVersion;
    st.ageTenths = clampValue<uint32_t>(st.ageTenths, 0, cfg.maxAgeTenths);
    st.hungry_value = clampValue<int32_t>(st.hungry_value, 0, cfg.max_hunger);
    st.mood = clampValue<int32_t>(st.mood, 0, cfg.max_mood);
    st.clean_value = clampValue<int32_t>(st.clean_value, 0, cfg.max_clean);
    st.env_value = clampValue<int32_t>(st.env_value, 0, cfg.max_env_clean);
    st.health = clampValue<int32_t>(st.health, 0, 100);

    char appearanceCode[9] = {};
    if (!copyAppearanceCode(appearanceCode, sizeof(appearanceCode), st.species))
        strcpy(st.species, "dino");
    if (!copyAppearanceCode(appearanceCode, sizeof(appearanceCode), st.outfit))
        strcpy(st.outfit, "base");

    return true;
}
