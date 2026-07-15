#include "pet/adapters/PetStorage.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

namespace
{
constexpr const char *kStateSlotAPath = "/state_a.bin";
constexpr const char *kStateSlotBPath = "/state_b.bin";
constexpr uint16_t kLegacyFloatAgeVersion = 7;
constexpr uint16_t kLegacyHealthyDaysVersion = 8;
constexpr uint16_t kLegacyStoredStatusVersion = 9;

struct LegacyPersistedPetState
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
    int16_t customStats[kPetCustomStatCount];
    uint32_t flowFlags;
    uint32_t crc32;
};

static_assert(sizeof(PersistedPetState) == 84, "Unexpected v10 pet state layout");
static_assert(sizeof(LegacyPersistedPetState) == 88, "Unexpected legacy pet state layout");

uint32_t legacyAgeTenthsFromFloatBits(uint32_t bits)
{
    // Version 7 stored age as a non-negative IEEE-754 float. Decode only the
    // bounded persisted value without introducing floating-point support again.
    if ((bits & 0x80000000UL) != 0)
        return 0;

    const uint32_t exponentBits = (bits >> 23) & 0xFFUL;
    if (exponentBits == 0)
        return 0;
    if (exponentBits >= 255 || exponentBits > 140)
        return UINT32_MAX;

    const int32_t exponent = static_cast<int32_t>(exponentBits) - 127;
    const int32_t denominatorShift = 23 - exponent;
    const uint32_t significand = (1UL << 23) | (bits & 0x7FFFFFUL);
    const uint32_t scaled = significand * Pet::kAgeScale;

    if (denominatorShift <= 0)
    {
        if (denominatorShift < -4)
            return UINT32_MAX;
        return scaled << static_cast<uint32_t>(-denominatorShift);
    }
    if (denominatorShift >= 32)
        return 0;

    return (scaled + (1UL << static_cast<uint32_t>(denominatorShift - 1))) >> static_cast<uint32_t>(denominatorShift);
}

uint32_t crc32Bitwise(const uint8_t *data, size_t length)
{
    uint32_t crc = 0xFFFFFFFFUL;
    for (size_t i = 0; i < length; ++i)
    {
        crc ^= data[i];
        for (uint8_t bit = 0; bit < 8; ++bit)
        {
            const uint32_t mask = (crc & 1UL) ? 0xEDB88320UL : 0;
            crc = (crc >> 1) ^ mask;
        }
    }
    return ~crc;
}

uint32_t calculateStateCrc(const PersistedPetState &state)
{
    return crc32Bitwise(reinterpret_cast<const uint8_t *>(&state), offsetof(PersistedPetState, crc32));
}

uint32_t calculateLegacyStateCrc(const LegacyPersistedPetState &state)
{
    return crc32Bitwise(reinterpret_cast<const uint8_t *>(&state), offsetof(LegacyPersistedPetState, crc32));
}

bool isSequenceNewer(uint32_t candidate, uint32_t current)
{
    return static_cast<int32_t>(candidate - current) > 0;
}

bool migrateLegacyState(const LegacyPersistedPetState &legacy, PersistedPetState &state)
{
    if (legacy.magic != Pet::kPetStateMagic ||
        (legacy.version != kLegacyFloatAgeVersion &&
         legacy.version != kLegacyHealthyDaysVersion &&
         legacy.version != kLegacyStoredStatusVersion))
    {
        return false;
    }

    if (calculateLegacyStateCrc(legacy) != legacy.crc32)
        return false;

    state = {};
    state.magic = legacy.magic;
    state.version = Pet::kPetStateVersion;
    state.hasSick = legacy.hasSick;
    state.sequence = legacy.sequence;
    state.ageTenths = legacy.version == kLegacyFloatAgeVersion
                          ? legacyAgeTenthsFromFloatBits(legacy.ageTenths)
                          : legacy.ageTenths;
    state.hungry_value = legacy.hungry_value;
    state.mood = legacy.mood;
    state.clean_value = legacy.clean_value;
    state.env_value = legacy.env_value;
    state.flowFlags = legacy.flowFlags;
    memcpy(state.customStats, legacy.customStats, sizeof(state.customStats));
    memcpy(state.species, legacy.species, sizeof(state.species));
    memcpy(state.outfit, legacy.outfit, sizeof(state.outfit));

    if (legacy.version == kLegacyFloatAgeVersion || legacy.version == kLegacyHealthyDaysVersion)
    {
        // Versions 7 and 8 stored healthy_days followed by stage_healthy_days
        // at the offsets later occupied by stage_days and health.
        const uint32_t legacyStageHealthyDays = static_cast<uint32_t>(legacy.health);
        state.stage_days = legacyStageHealthyDays;
        state.health = legacyStageHealthyDays > 100U
                           ? 100
                           : static_cast<int32_t>(legacyStageHealthyDays);
    }
    else
    {
        state.stage_days = legacy.stage_days;
        state.health = legacy.health;
    }

    return true;
}

bool readStateSlot(SdFat *sd, const char *path, PersistedPetState &state)
{
    File f = sd->open(path, FILE_READ);
    if (!f)
        return false;

    const size_t stateSize = f.size();
    if (stateSize == sizeof(LegacyPersistedPetState))
    {
        LegacyPersistedPetState legacy = {};
        const size_t readCount = f.read(reinterpret_cast<uint8_t *>(&legacy), sizeof(legacy));
        f.close();
        return readCount == sizeof(legacy) && migrateLegacyState(legacy, state);
    }

    if (stateSize != sizeof(PersistedPetState))
    {
        f.close();
        return false;
    }

    state = {};
    const size_t readCount = f.read(reinterpret_cast<uint8_t *>(&state), stateSize);
    f.close();

    if (readCount != stateSize)
        return false;

    if (state.magic != Pet::kPetStateMagic)
        return false;

    if (calculateStateCrc(state) != state.crc32)
        return false;

    return state.version == Pet::kPetStateVersion;
}

bool writeStateSlot(SdFat *sd, const char *path, const PersistedPetState &state)
{
    File f = sd->open(path, O_WRONLY | O_CREAT | O_TRUNC);
    if (!f)
        return false;

    const size_t n = f.write(reinterpret_cast<const uint8_t *>(&state), sizeof(state));
    f.flush();
    f.close();

    return n == sizeof(state);
}
} // namespace

PetStorage::PetStorage(SdFat *ref_sd) : sd(ref_sd) {}

bool PetStorage::save(const Pet &pet)
{
    attemptedSaveSlot = writeSlotA ? 'A' : 'B';
    attemptedSaveSequence = nextSequence;

    if (sd == nullptr)
        return false;

    PersistedPetState state = pet.persistentState();
    state.sequence = nextSequence;
    state.crc32 = calculateStateCrc(state);

    const char *path = writeSlotA ? kStateSlotAPath : kStateSlotBPath;
    if (!writeStateSlot(sd, path, state))
        return false;

    ++nextSequence;
    writeSlotA = !writeSlotA;
    return true;
}

char PetStorage::lastSaveSlot() const
{
    return attemptedSaveSlot;
}

uint32_t PetStorage::lastSaveSequence() const
{
    return attemptedSaveSequence;
}

bool PetStorage::load(Pet &pet)
{
    if (sd == nullptr)
        return false;

    PersistedPetState stateA = {};
    PersistedPetState stateB = {};
    const bool hasA = readStateSlot(sd, kStateSlotAPath, stateA);
    const bool hasB = readStateSlot(sd, kStateSlotBPath, stateB);

    if (!hasA && !hasB)
        return false;

    const PersistedPetState &selected = (!hasB || (hasA && isSequenceNewer(stateA.sequence, stateB.sequence)))
                                           ? stateA
                                           : stateB;
    const bool selectedSlotA = (&selected == &stateA);
    if (!pet.restoreState(selected))
        return false;

    nextSequence = selected.sequence + 1;
    writeSlotA = !selectedSlotA;
    return true;
}
