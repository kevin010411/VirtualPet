#include "Pet.h"

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
    st.age = clamp<float>(age, 0.0f, cfg.max_age);
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

void Pet::dayPassed()
{
    if (getStatus() != HealthStatus::Healthy)
        return;

    st.hungry_value = clamp<int>(st.hungry_value + 1, 0, cfg.max_hunger);
    st.mood = clamp<int>(st.mood - 1, 0, cfg.max_mood);
    st.age = clamp<float>(st.age + cfg.age_per_tick, 0.0f, cfg.max_age);
    st.clean_value = clamp<int>(st.clean_value - 1, 0, cfg.max_clean);
    st.env_value = clamp<int>(st.env_value - 1, 0, cfg.max_env_clean);
    refreshStatus();
}

void Pet::feedPet(int add_satiety)
{
    st.hungry_value = clamp<int>(st.hungry_value - add_satiety, 0, cfg.max_hunger);
    refreshStatus();
}

void Pet::changeMood(int delta)
{
    st.mood = clamp<int>(st.mood + delta, 0, cfg.max_mood);
    refreshStatus();
}

void Pet::takeShower(int value)
{
    st.clean_value = clamp<int>(st.clean_value + value, 0, cfg.max_clean);
    refreshStatus();
}

void Pet::cleanEnv(unsigned int clear_value)
{
    st.env_value = clamp<int>(st.env_value + clear_value, 0, cfg.max_env_clean);
    refreshStatus();
}

void Pet::decayEnvironment(unsigned int decay_value)
{
    st.env_value = clamp<int>(st.env_value - static_cast<int>(decay_value), 0, cfg.max_env_clean);
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
    const double normalized = st.age / cfg.max_age;
    if (normalized < 0.3)
        return AnimationId::StatusAge01;
    if (normalized < 0.75)
        return AnimationId::StatusAge05;
    return AnimationId::StatusAge1;
}

void Pet::setDefaultState()
{
    st.magic = MAGIC;
    st.version = VER;
    st.hasSick = false;
    st.status = static_cast<uint8_t>(HealthStatus::Healthy);
    st.age = 0.0f;
    st.hungry_value = 0;
    st.mood = 70;
    st.clean_value = 200;
    st.env_value = 800;
}

String Pet::getAge()
{
    switch (CurrentAgeAnimation())
    {
    case AnimationId::StatusAge01:
        return "0.1";
    case AnimationId::StatusAge05:
        return "0.5";
    case AnimationId::StatusAge1:
    default:
        return "1";
    }
}

HealthStatus Pet::getStatus() const
{
    return static_cast<HealthStatus>(st.status);
}

const PersisState &Pet::persistentState() const
{
    return st;
}

bool Pet::restoreState(const PersisState &state)
{
    if (state.magic != MAGIC || state.version != VER)
        return false;

    st = state;
    refreshStatus();
    return true;
}
