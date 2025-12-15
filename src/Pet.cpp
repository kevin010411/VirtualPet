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

Pet::Pet(Adafruit_ST7735 *ref_tft,SdFat *ref_SD, float age) :
                                                tft(ref_tft),SD(ref_SD) {}

void Pet::dayPassed()
{
    if(HealthStatus(st.status)!=HealthStatus::Healthy)
        return;
        
    st.hungry_value = clamp<int>(st.hungry_value + 1, 0, cfg.max_hunger);
    st.mood = clamp<int>(st.mood - 1, 0, cfg.max_mood);
    st.age = clamp<float>(st.age + cfg.age_per_tick, 0.0f, cfg.max_age);
    st.clean_value = clamp<int>(st.clean_value - 1, 0, cfg.max_clean);
    st.env_value = clamp<int>(st.env_value - 1, 0, cfg.max_env_clean);

    st.status = (uint8_t)decide_state(st.hungry_value, st.mood, st.env_value, st.clean_value, st.hasSick, cfg);
}

void Pet::feedPet(int add_satiety)
{
    st.hungry_value = clamp<int>(st.hungry_value - add_satiety, 0, cfg.max_hunger);
    st.status = (uint8_t)decide_state(st.hungry_value, st.mood, st.env_value, st.clean_value, st.hasSick, cfg);

}

void Pet::changeMood(int delta)
{
    st.mood = clamp<int>(st.mood + delta, 0, cfg.max_mood);
    st.status = (uint8_t)decide_state(st.hungry_value, st.mood, st.env_value, st.clean_value, st.hasSick, cfg);

}

void Pet::takeShower(int value)
{
    st.clean_value = clamp<int>(st.clean_value + value, 0, cfg.max_clean);
    st.status = (uint8_t)decide_state(st.hungry_value, st.mood, st.env_value, st.clean_value, st.hasSick, cfg);

}

void Pet::cleanEnv(unsigned int clear_value)
{
    st.env_value = clamp<int>(st.env_value + clear_value, 0, cfg.max_env_clean);
    st.status = (uint8_t)decide_state(st.hungry_value, st.mood, st.env_value, st.clean_value, st.hasSick, cfg);

}

void Pet::getSick()
{
    st.hasSick = true;
    st.status = (uint8_t)decide_state(st.hungry_value, st.mood, st.env_value, st.clean_value, st.hasSick, cfg);

}

bool Pet::takeMedicine()
{
    if (!st.hasSick)
        return false;
    st.hasSick = false;
    st.status = (uint8_t)decide_state(st.hungry_value, st.mood, st.env_value, st.clean_value, st.hasSick, cfg);

    return true;
}

String Pet::CurrentAnimation() const
{
    switch (HealthStatus(st.status))
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

bool Pet::saveStateToSD()
{
    File f = SD->open("/state.tmp",FILE_WRITE);
    if(!f) return false;

    st.magic = MAGIC; // "PET1"
    st.version = VER;

    f.seek(0);
    size_t n = f.write((uint8_t*)&st,sizeof(st));
    f.flush();
    f.close();

    if(SD->exists("/state.bin"))
        SD->remove("/state.bin");
    if(!SD->rename("/state.tmp","/state.bin")) 
        return false;

    return n==sizeof(st);
}

bool Pet::loadStateFromSD()
{
    File f = SD->open("/state.bin",FILE_READ);
    if(!f) return false;

    if(f.size()!=sizeof(PersisState))
    {
        f.close();
        return false;
    }

    f.read((uint8_t*)&st,sizeof(st));
    f.close();

    if(st.magic!=MAGIC || st.version != VER)
        return false;

    return true;
}

void Pet::setDefaultState()
{
    st.magic = MAGIC;
    st.version = VER;

    st.hasSick = false;

    st.age = 0.0f;

    st.hungry_value = 0;
    st.mood = 70;
    st.clean_value = 200;
    st.env_value = 800;
}